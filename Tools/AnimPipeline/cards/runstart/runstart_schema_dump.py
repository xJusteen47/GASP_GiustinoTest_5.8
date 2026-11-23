"""Read-only (runstart-phase): schemas of the internal pose search databases of CHT_GSTN_StockMover_PoseMatch (channels, weights,
bones, sample times) and the database settings that steer loop entry. Writes Saved/AnimPipeline/rows/runstart_schema_dump.txt."""
import os
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_schema_dump.txt")
L = []
seen = set()


def dump_obj(o, ind, depth=0):
    if depth > 4 or o is None:
        return
    for p in ("weight", "bone", "sample_time_offset", "sample_role", "feature", "sample_times", "channel_name", "curve_name",
              "input_query_pose", "component_stripping", "debug_color", "permutation_time_type", "use_feature", "space"):
        try:
            L.append("%s%s=%s" % (ind, p, str(o.get_editor_property(p))[:200]))
        except Exception:
            pass
    for sub in ("channels", "sub_channels"):
        try:
            ch = o.get_editor_property(sub)
        except Exception:
            continue
        for c in ch:
            L.append("%s- %s" % (ind, c.get_class().get_name() if c else None))
            dump_obj(c, ind + "    ", depth + 1)


# internal databases are sub-objects of the chooser package: find them through the PoseSearchColumn cells
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
import re
txt = open(os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_phase_dump.txt"), encoding="utf-8").read()
for m in sorted(set(re.findall(r"Database=\"/Script/PoseSearch.PoseSearchDatabase'([^']+)'", txt))):
    db = unreal.load_object(None, m)
    if not db:
        L.append("DB %s: not loaded" % m); continue
    L.append("== DB %s" % m)
    for p in ("schema", "continuing_pose_cost_bias", "base_cost_bias", "looping_cost_bias", "pose_search_mode", "tags", "normalization_set"):
        try:
            L.append("  %s=%s" % (p, str(db.get_editor_property(p))[:200]))
        except Exception as e:
            pass
    sch = db.get_editor_property("schema")
    if sch and sch.get_path_name() not in seen:
        seen.add(sch.get_path_name())
        L.append("  SCHEMA %s" % sch.get_path_name())
        for p in ("sample_rate", "skeletons", "data_preprocessor", "number_of_permutations", "permutations_sample_rate"):
            try:
                L.append("    %s=%s" % (p, str(sch.get_editor_property(p))[:200]))
            except Exception:
                pass
        dump_obj(sch, "    ")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(L) + "\n")
print("ESITO: OK lines %d" % len(L))
