#!/usr/bin/env node
// Async job runner for the Antigravity CLI (agy). Zero dependencies, Node >= 18.
// Replaces long blocking agy-bridge `delegate` calls: submit returns at once, the
// job runs detached with its own long timeout, everything lands in files.
//
//   node agy-job.mjs submit --prompt-file p.md [--cwd DIR] [--model M] [--timeout 3600] [--retries 1] [--conversation ID]
//   node agy-job.mjs status <id>     -> one line: <state> [reason]   (exit 0 done, 2 running, 1 failed)
//   node agy-job.mjs result <id>     -> prints result path (and first 40 lines) when done
//   node agy-job.mjs wait <id> [--max 120]   -> poll max N seconds (no Claude tokens spent between polls)
//   node agy-job.mjs cancel <id> | list
//
// Jobs live in $AGY_JOBS_DIR or <cwd>/.agy-jobs/<id>/ : prompt.md meta.json result.md stderr.txt agy.log
// States: running | done | empty | quota | timeout | error | cancelled
import { spawn, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { randomBytes } from 'node:crypto';

const [cmd, ...rest] = process.argv.slice(2);
const opt = {}; const pos = [];
for (let i = 0; i < rest.length; i++) {
  if (rest[i].startsWith('--')) opt[rest[i].slice(2)] = rest[++i]; else pos.push(rest[i]);
}
const root = () => path.resolve(process.env.AGY_JOBS_DIR || path.join(opt.cwd || process.cwd(), '.agy-jobs'));
const jobDir = (id) => path.join(root(), id);
const readMeta = (id) => JSON.parse(fs.readFileSync(path.join(jobDir(id), 'meta.json'), 'utf8'));
const writeMeta = (id, m) => { const f = path.join(jobDir(id), 'meta.json'); fs.writeFileSync(f + '.tmp', JSON.stringify(m, null, 1)); fs.renameSync(f + '.tmp', f); };
const alive = (pid) => { try { process.kill(pid, 0); return true; } catch { return false; } };
const AGY = process.env.AGY_PATH || 'agy';

function submit() {
  const pf = opt['prompt-file'];
  if (!pf) die('submit needs --prompt-file');
  const id = new Date().toISOString().replace(/[-:T]/g, '').slice(0, 14) + '-' + randomBytes(2).toString('hex');
  fs.mkdirSync(jobDir(id), { recursive: true });
  fs.copyFileSync(pf, path.join(jobDir(id), 'prompt.md'));
  const meta = {
    id, state: 'running', created: new Date().toISOString(), cwd: path.resolve(opt.cwd || process.cwd()),
    model: opt.model || null, conversation: opt.conversation || null,
    timeoutSec: Number(opt.timeout || 3600), retries: Number(opt.retries ?? 1), attempt: 0,
  };
  writeMeta(id, meta);
  const child = spawn(process.execPath, [new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1'), '_run', id],
    { detached: true, stdio: 'ignore', windowsHide: true, env: { ...process.env, AGY_JOBS_DIR: root() } });
  child.unref();
  console.log(id);
}

function runAgy(id, m) {
  const d = jobDir(id);
  const log = path.join(d, 'agy.log');
  const args = [];
  if (process.env.AGY_SKIP_PERMISSIONS !== 'false') args.push('--dangerously-skip-permissions');
  if (process.env.AGY_SANDBOX === 'true') args.push('--sandbox');
  args.push('--add-dir', m.cwd, '--log-file', log);
  if (m.conversation) args.push('--conversation', m.conversation);
  if (m.model) args.push('--model', m.model);
  args.push('--print-timeout', `${m.timeoutSec}s`, '-p', fs.readFileSync(path.join(d, 'prompt.md'), 'utf8'));
  return new Promise((resolve) => {
    const out = fs.openSync(path.join(d, 'result.md'), 'w');
    const err = fs.openSync(path.join(d, 'stderr.txt'), 'w');
    const c = spawn(AGY, args, { cwd: m.cwd, stdio: ['ignore', out, err], windowsHide: true, shell: process.platform === 'win32' && !/\.exe$/i.test(AGY) });
    m.agyPid = c.pid; writeMeta(id, m);
    const killTimer = setTimeout(() => { c.kill('SIGKILL'); resolve({ code: null, timedOut: true }); }, (m.timeoutSec + 30) * 1000);
    c.on('error', (e) => { clearTimeout(killTimer); resolve({ code: -1, spawnError: e.message }); });
    c.on('close', (code) => { clearTimeout(killTimer); resolve({ code }); });
  });
}

async function run(id) {
  const d = jobDir(id); const m = readMeta(id); m.runnerPid = process.pid;
  for (m.attempt = 1; m.attempt <= m.retries + 1; m.attempt++) {
    writeMeta(id, m);
    const r = await runAgy(id, m);
    const text = fs.readFileSync(path.join(d, 'result.md'), 'utf8').trim();
    const log = fs.existsSync(path.join(d, 'agy.log')) ? fs.readFileSync(path.join(d, 'agy.log'), 'utf8') : '';
    const quota = /RESOURCE_EXHAUSTED|code 429/.test(log) ? (log.match(/Resets in ([0-9hms ]+)/) || [])[1] || 'unknown' : null;
    if (readMeta(id).state === 'cancelled') return;
    if (r.spawnError) return finish(id, m, 'error', `cannot start agy: ${r.spawnError} (set AGY_PATH)`);
    if (r.timedOut) return finish(id, m, 'timeout', `no exit after ${m.timeoutSec}s`);
    if (quota) return finish(id, m, 'quota', `quota exhausted, resets in ${quota}`);
    if (r.code !== 0) {
      const e = fs.readFileSync(path.join(d, 'stderr.txt'), 'utf8').trim().slice(0, 300);
      return finish(id, m, 'error', `agy exit ${r.code}: ${e}`);
    }
    if (text) return finish(id, m, 'done', `${text.length} chars, attempt ${m.attempt}`);
    // empty reply: retry (cheap for agy), then report clearly
  }
  m.attempt = m.retries + 1;
  finish(id, m, 'empty', `agy returned nothing after ${m.attempt} attempts (check permissions/sandbox flags, see agy.log)`);
}
function finish(id, m, state, reason) { m.state = state; m.reason = reason; m.finished = new Date().toISOString(); writeMeta(id, m); }

function state(id) {
  const m = readMeta(id);
  // runner died without finishing (machine slept, killed): do not leave it "running" forever
  if (m.state === 'running' && m.runnerPid && !alive(m.runnerPid)) { m.state = 'error'; m.reason = 'runner died; resubmit'; writeMeta(id, m); }
  return m;
}
function status() { const m = state(pos[0]); console.log(`${m.state}${m.reason ? ' — ' + m.reason : ''}`); process.exit(m.state === 'running' ? 2 : m.state === 'done' ? 0 : 1); }
function result() {
  const m = state(pos[0]);
  if (m.state !== 'done') { console.log(`${m.state}${m.reason ? ' — ' + m.reason : ''}`); process.exit(m.state === 'running' ? 2 : 1); }
  const p = path.join(jobDir(pos[0]), 'result.md');
  console.log(p); console.log(fs.readFileSync(p, 'utf8').split('\n').slice(0, 40).join('\n'));
}
async function wait() {
  const end = Date.now() + Number(opt.max || 120) * 1000;
  while (Date.now() < end && state(pos[0]).state === 'running') await new Promise((r) => setTimeout(r, 5000));
  status();
}
function cancel() {
  const m = readMeta(pos[0]); m.state = 'cancelled'; writeMeta(pos[0], m);
  if (m.agyPid) { try { process.kill(m.agyPid, 'SIGKILL'); } catch {} }
  console.log('cancelled');
}
function list() {
  if (!fs.existsSync(root())) return;
  for (const id of fs.readdirSync(root()).sort()) { try { const m = state(id); console.log(`${id}  ${m.state}  ${m.reason || ''}`); } catch {} }
}
function die(s) { console.error(s); process.exit(64); }

({ submit, _run: () => run(pos[0]), status, result, wait, cancel, list }[cmd] || (() => die('usage: submit|status|result|wait|cancel|list')))();
