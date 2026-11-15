# Antigravity (agy) from Claude threads: timeouts, empty replies, async jobs

Bridge = public npm package `agy-bridge` 0.5.0 (MCP tools `delegate`, `follow_up`, ...), registered in
`C:\Users\jacop\.claude.json` on the desktop. Source read 2026-11-15; it is NOT in this repo.

## Why calls failed (from the package source)
1. "cut at 10 min": two layers. The MCP client timeout in `.claude.json` (`"timeout": 600000` = 10 min) gives up first,
   with `timed out waiting for response`. agy keeps running; `follow_up` with the returned session id can fetch the result.
   agy-bridge's own cut reads `agy timed out after Ns`; `delegate` default is 600 s.
2. "empty reply": agy exits 0 with no text when (a) it hit its print-timeout, (b) permissions are not skipped in headless
   mode (`AGY_SKIP_PERMISSIONS=false` was set on 11-11), (c) quota 429 (the bridge detects this one and fails over).
3. Sandbox (`AGY_SANDBOX=true`) probably blocks Unreal remote execution used by `animpipe build`.

## Fix A: config (owner, 1 minute)
In the `agy-bridge` entry of `.claude.json` set: `"timeout": 1800000`, env `AGY_TIMEOUT_DELEGATE=1500`,
`AGY_SKIP_PERMISSIONS=true`, `AGY_SANDBOX=false`, `AGY_ON_FAILURE=strict` (stops Claude doing agy's job after a failure,
which burns Claude tokens). Restart the session. Rule: client timeout >= agy budget.

## Fix B: async jobs, no waiting (Tools/AgyJobs/agy-job.mjs, Node only)
Run from the desktop shell; submit returns at once, the job runs detached with a long timeout (default 3600 s),
everything is in files under `.agy-jobs/<id>/` (`result.md`, `stderr.txt`, `agy.log`, `meta.json`).
```
node Tools/AgyJobs/agy-job.mjs submit --prompt-file prompt.md --cwd <worktree> [--model M] [--timeout 3600] [--retries 1]
node Tools/AgyJobs/agy-job.mjs wait <id> --max 120   # polls in the script, costs no Claude tokens; exit 0 done, 2 running, 1 failed
node Tools/AgyJobs/agy-job.mjs result <id>           # path of result.md + first 40 lines
node Tools/AgyJobs/agy-job.mjs cancel <id> | list
```
States are explicit: `done | empty | quota (with reset time) | timeout | error | cancelled`. An empty reply is retried
(`--retries`, default 1) then reported as `empty`, never as success. Add `.agy-jobs/` to .gitignore.
Tested here only with a fake `agy` (done / empty / quota paths). NOT tested with the real agy.

## Prompt guidance (small tasks)
One clip or one question per job; tell agy to write its output to a named file in the worktree and reply with one line;
Claude reads the file, never the whole reply. Retries are cheap on the agy side, so resubmit rather than enlarge a prompt.

## Desktop test (later checkpoint, needs a free editor slot only if the prompt uses Unreal)
1. Put `Tools/AgyJobs/agy-job.mjs` on the desktop (merge this branch). 2. Prompt "write OK to ok.txt".
3. `submit`, `wait --max 120`, check `result` and `.agy-jobs/<id>/agy.log`.
