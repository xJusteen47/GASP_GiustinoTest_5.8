# Handover: agy bridge timeouts (2026-11-15) — cloud, Sonnet medium, ~$2

Done: root cause read from agy-bridge 0.5.0 source (`npm pack agy-bridge`, dist/index.js lines 287-420, README Timeouts);
async wrapper Tools/AgyJobs/agy-job.mjs; docs/notes/agy-bridge.md (config fix + usage).
Verified: fake-agy test in the cloud: done, empty (retry then `empty`), quota (reset time) all report correctly.
Not verified: real agy on desktop, Windows spawn path (`shell` for non-.exe AGY_PATH), Windows `import.meta.url` path handling.
Choices applied: wrapper instead of forking the bridge (public package, no async mode upstream).
To confirm/owner: edit `.claude.json` per docs/notes/agy-bridge.md "Fix A"; desktop test is a later checkpoint.
Branch claude/project-thread-p2kkgc.
