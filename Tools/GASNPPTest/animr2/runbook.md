NOTE: the scripts in this runbook live in Tools/GASNPPTest/animr2/ (versioned copy).
Copy them to Saved/AnimR2/ and run them from there; the paths inside the scripts and below are unchanged.
Saved/ is not in the repo, so a new machine needs this copy first (and Saved/AnimR2/logs/).
# Runbook (worked 2026-11-15, dropkick). P="C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59"; run from $P in Git Bash.
1. Boot file (no spaces in its path; mover logs to Saved/AnimR2/logs/vd.log). Already at
   C:/Users/jacop/AppData/Local/Temp/claude/8c81644554604783/a744393f-c912-4667-b073-01ffe1248c78/scratchpad/boot_vd2.py (recreate if gone):
   `import os,runpy; os.environ['VD_LOG']=r'<P>/Saved/AnimR2/logs/vd.log'; runpy.run_path(r'<P>/Tools/GASNPPTest/vd2_mover.py', run_name='vd2_mover')`
2. Start the editor (L_Test_Hub, windows go to desktop 2 by themselves, hr=0; the PowerShell call blocks, so run it in background):
   powershell -NoProfile -Command "Start-Process 'C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"<P windows path>\GASP_Giustino2.uproject\"','/Game/Levels/Test/L_Test_Hub','-ExecCmds=\"py <boot_vd2.py path>\"','-nosplash'"
   Ready after ~60-90 s when `curl -s -m3 -o /dev/null -w "%{http_code}" http://127.0.0.1:8000/mcp` answers 405 (built-in MCP, what animpipe uses).
3. Python inside the editor (any file, stdout kept, <=3 lines printed):  python Saved/AnimR2/run_in_editor.py <script.py> <log.txt>
   e.g. python Saved/AnimR2/run_in_editor.py Tools/AnimPipeline/assemble_dropkick_montage.py Saved/AnimR2/logs/assemble.txt
   kit:  python Saved/AnimR2/run_in_editor.py Tools/GAS/setup_overlord_dropkick_kit.py Saved/AnimR2/logs/kit.txt   (OVERLORD_DROPKICK_REFRESH: edit run(refresh=True))
4. animpipe (editor must be up; pass the card PATH; ONE json-ish line each, redirect, print head -2):
   cd Tools/AnimPipeline; for c in startup loop land; do python -m animpipe author cards/overlord/dropkick/overlord_dropkick_$c.json > ../../Saved/AnimR2/logs/author_$c.txt 2>&1; done   (same for build, check)
   check detail per card: Saved/AnimPipeline/animpipe/<card>_check.txt (grep "block ... FAIL"). Needs Saved/AnimPipeline/refs/ufd/*.csv first.
5. Play capture (editor up, L_Test_Hub open, nothing else running in it). Returns at once; takes ~70 s; the script ends Play and QUITS the editor:
   python Saved/AnimR2/run_capture.py <KIT_name> <out_dir> [seq1,seq2]      e.g. KIT_Test_OverlordDropkick Saved/AnimR2/dropkick_smoke/cap
   Wait for <out_dir>/done.txt = "ok" (tail <out_dir>/capture.log). Kit name picks the sequences (Dropkick / DAir / default jab); PIE_CAPTURE_SEQUENCES in arg 3.
   Base vs variant: run step 5 twice with out dirs <x>/base and <x>/variant (editor restarts each time: step 2), base BEFORE the rebuild.
6. Outside the editor: state per frame  python Tools/GASNPPTest/pie_capture_grid.py state <out_dir> <seq>
   mp4 (7 views, 4x2):  python Tools/GASNPPTest/pie_capture_grid.py video <out_dir> <seq> <out.mp4> --from=30 --to=125
   one view strip:      ... sheet <out_dir> <seq> game <out.png> --from=F --to=F --cols=10 --cell=200 --step=N
   composite (game=behind + az90=side, chosen frames): python Saved/AnimR2/make_composite.py <out_dir> <seq> <out.png>  (edit the frames list inside)
   Per-row montage position: frames.jsonl field 'montage' = [name, animation frame]; 'h' = height above the standing z.
7. After EVERY editor run: git status --short ; git checkout -- Plugins/GSTNPiggyback/Content/Test/PM_BoostJump.uasset Plugins/GSTNPiggyback/Content/Test/PM_ThrowRider.uasset
   (and IC_GASNPP_Test / KIT_Test_Hits if listed). Never touch AM_OverlordDAirV9.uasset or "Apri editor 5.9.bat". Never save maps.
8. Close: a capture quits the editor itself. Without a capture: python Saved/AnimR2/run_in_editor.py <file with unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')>
   or Stop-Process -Name UnrealEditor. Verify: tasklist | grep -c -i UnrealEditor  -> 0.
Gotchas: AnimSegment.start_pos is read-only in UE 5.9 (set it with struct.import_text, see assemble_dropkick_montage.py); a montage's length only updates after
save + EditorLoadingAndSavingUtils.reload_packages(...ASSUME_POSITIVE). Dropkick capture sequence = dropkick_whiff (jump, stick forward at 150 cm rising, Attack next frame).
