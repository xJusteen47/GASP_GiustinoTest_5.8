# Handover ue59-adoption-1 (2026-11-19, desktop, no editor, worktree GASP_Giustino2_m11v, branch claude/ue59-adoption-1 from 7f6c441c)

Result: offline checks done, nothing in 5.9 breaks our setup; adoption assessed. Full table: docs/notes/ue59-adoption.md.

Done
- Python packages, init_unreal.py, MCP Tool Library, MCP settings: no impact (evidence in the doc, scans in _scratch/ue59_scan*.txt, made by _scratch/ue59_scan.py and ue59_scan2.py).
- Control Rig: 16 of 94 rig assets have candidate deprecated nodes (byte scan, may over-report); ours to fix: CR_WallIkControlRig, CR_Biped_FootPlacement.
- Frame-rate cap: no ini change 5.8 -> 5.9; runtime values pending the editor script.
- Adoption: UAF Niagara notify NO (needs UAF runtime, our dust is event driven); Chooser Python YES in tools; Pose Search roles LATER (grabs/throws); BP DSL YES for plain BPs, StateTree SKIP; UBA Cloud NO.
- speed-up.md: power plan note added (owner report).

- Unreal Assistant: programmatic recipe Tools/UE59/ask_unreal_assistant.py (MCP execute_python -> AIAssistantInstance, reply to Saved/UE59/*.json); tool access default Auto, suggest Read; external agent mode kept OFF (doc §5).
- ML candidates (AnimGen/UAFAnimGen need UAF; RigMapper; ML Deformer): not adopted (doc §6).

Decisions applied (rule 6)
- No Toolset Registry restriction and no MCP Instructions applied: snippets ready in §3 of the doc.
- Branch based on claude/anim-setup-2 (7f6c441c), not origin/main (1420 commits behind).

Open
- Editor thread: run Tools/UE59/ue59_editor_checks.py, then ue59_ask_assistant.py (Editor AI assistant, free) (read-only, ~1 min), copy Saved/UE59/ue59_editor_checks.txt to docs/notes/.
- After that: confirm toolset block list; code thread for deprecated nodes on our two rigs.
- Leftover empty branch claude/ue59-adoption (= origin/main) in m11v: deleting it was blocked, harmless.
Cost estimate: ~6 $.
