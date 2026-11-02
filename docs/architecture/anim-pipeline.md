# Animation pilot pipeline (Cascadeur -> Unreal)

Tooling for the combat-move animation pilot lives in [`Tools/AnimPipeline/`](../../Tools/AnimPipeline/README.md): motion
models, pre-flight and quality checks (motion, legibility, tangency, balance, clearance), the Cascadeur and Unreal
round-trip scripts and the light-model fix loop. The README there explains the purpose, how to run a pass and how to
extend it. It is a content/animation workflow, not game code; nothing in the C++ modules depends on it.

Data (scenes, renders, references, logs) stays out of git, in the working folder `Saved/AnimPipeline/`
(`ANIMPIPELINE_DIR`); `Tools/AnimPipeline/sync.sh` moves the tracked files between the two.
