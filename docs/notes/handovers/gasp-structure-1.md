# Handover gasp-structure-1 (2026-11-20, research only, desktop, no asset changed)

Goal: reference of stock GASP Mover 5.9 (ABP state machine, choosers, pose search, exits, root motion, feet, curves) + good
practices to extend it + diff vs our setup. Result: docs/knowledge/gasp/gasp-structure-5.9.md (agent index at the top),
row in docs/knowledge/INDEX.md, compact evidence in docs/knowledge/gasp/evidence/.

How it was read (all read-only):
- Headless dump, UE 5.9, project GASP_Giustino2_ue59: Tools/GAS/routing/gasp_structure_dump.py (+ gasp_cht_fmt.py).
  Runs: Saved/GaspStructure/dump_stock.log, dump_stock2.log (ONLY=choosers), dump_ours.log (ABP_GSTN_Locomotion,
  CHT_GSTN_*), all "ESITO: OK" (.esito files). Outputs Saved/GaspStructure/{stock,stock2,ours}_*.txt (not committed, ~5 MB).
- Unreal Assistant: Tools/UE59/ask_assistant_boot.py through Run-Timed -Preset Capture (editor on VD2, quits itself);
  questions Saved/Assistant/questions.json, replies copied to docs/knowledge/gasp/evidence/assistant_replies.md.
  Previous replies.json kept as Saved/Assistant/replies_prev_backup.json.

Numbers and where they come from:
- 61 chooser tables, 38 with a PoseSearchColumn: `grep -c "^=== " Saved/GaspStructure/stock_cht.txt`,
  `grep -E "^ col" stock_cht.txt | awk '{print $2}' | sort | uniq -c`.
- 10 states / 39 transitions stock: `grep -c "^STATE\|^TR" Saved/GaspStructure/stock_abp_sm.txt` (both machines incl. Ragdoll).
- Shared nested tables identical in row count stock vs ours: diff script in this thread (table/rows/cols compare, output in
  the doc section 9).
- Notify counts (BlockTransition 666, ExcludeFromDatabase 769, BranchIn 387, EarlyTransition 23 clips, Foley 1284):
  `grep -rl <ClassName> --include=*.uasset Content/Characters/UEFN_Mannequin/Animations | wc -l`.

Decisions applied (rule 6): base branch claude/anim-setup-3 (the architecture draft lives there); INDEX.md copied from
claude/project-thread-m6s9y2 (knowledge-base thread) with one added row: merging both branches gives an add/add conflict on
that file, resolve by keeping their file + our row.

Correction to earlier notes: docs/notes/anim-setup-audit.md section 1 says motion matching picks idle/walk/run; in the Mover
setup the chooser picks and the PoseSearchColumn only picks clip+frame among filtered rows (doc section 0).

Open (doc section 11): PoseSearchColumn output binding of StartTime, schemas of the 6 internal databases, exact foot-curve
semantics in CR_Biped_FootPlacement. A clean stock ABP must come from the sample: SandboxCharacter_Mover_ABP in this project
already holds our GSTN_* nodes and layer slots.
Cost: estimate in the thread reply.
