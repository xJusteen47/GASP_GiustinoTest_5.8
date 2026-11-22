"""Read-only: every transition into or out of Skid (and its alias) in ABP_GSTN_Locomotion, with rule and crossfade.
Headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script=<copy without spaces> -nullrhi. Report Saved/Skid/exit_dump.txt"""
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "Skid/exit_dump.txt"
log = []
G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
try:
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    for info in G.call_method("FindStateMachines", (abp, "")):
        d = G.call_method("GetStateMachine", (info.machine,))
        if not any(s.name == "Skid" for s in d.states):
            continue
        log.append(f"machine {d.name} entry {d.entry_state}")
        for s in d.states:
            if "kid" in s.name:
                log.append(f"state {s.name} kind {s.kind} anims {[a.get_name() for a in s.animations]}")
        for t in d.transitions:
            if "kid" in t.from_ or "kid" in t.to:
                log.append(f"{t.from_} -> {t.to} xf {t.crossfade_duration} rule {t.rule}")
except Exception:
    import traceback
    log.append(traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
