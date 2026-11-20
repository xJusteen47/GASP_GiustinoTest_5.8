"""Read-only dump of what sets the run reversal time (skid-reversal-1): the GASP walking mode CDO and DA_Locomotion_Default.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -nullrhi. Writes Saved/Reversal/reversal_dump.txt. Prints max 5 lines.
"""
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Reversal", "reversal_dump.txt")
WALK = os.environ.get("REV_WALK", "/Game/Blueprints/MovementModes/BP_MovementMode_Walking")
PROFILE = os.environ.get("REV_PROFILE", "/Game/GASNPPTest/Combat/DA_Locomotion_Default")
NAMES = ["MaxSpeed", "Acceleration", "Deceleration", "DirectionalAccelerationFactor", "TurningStrength", "AccelerationSmoothingTime",
         "DecelerationSmoothingTime", "AccelerationSmoothingCompensation", "DecelerationSmoothingCompensation", "OutsideInfluenceSmoothingTime",
         "FacingSmoothingTime", "VelocityDeadzoneThreshold", "WalkSpeed", "RunSpeed", "SprintSpeed", "WalkAcceleration", "RunAcceleration",
         "SprintAcceleration", "SprintTurnStrength", "WalkDeceleration", "RunDeceleration", "SprintDeceleration"]

log = []
cls = unreal.load_object(None, WALK + ".BP_MovementMode_Walking_C")
cdo = unreal.get_default_object(cls)
log.append("walk %s parent=%s" % (WALK, cls.get_super_class().get_name() if hasattr(cls, "get_super_class") else "?"))
for n in NAMES:
    for cand in (n, unreal.StringLibrary if False else n):
        try:
            log.append("  %s = %s" % (n, cdo.get_editor_property(n)))
        except Exception:
            pass
        break
# Blueprint variables can have other names: list every float-ish property the editor exposes
try:
    for p in sorted(dir(cdo)):
        if any(k in p.lower() for k in ("speed", "accel", "decel", "turn", "brak", "smooth", "friction")):
            try:
                log.append("  [dir] %s = %s" % (p, getattr(cdo, p)))
            except Exception:
                pass
except Exception as e:  # noqa: BLE001
    log.append("dir failed %r" % e)

prof = unreal.load_asset(PROFILE)
log.append("profile %s" % PROFILE)
rs = prof.get_editor_property("run_stages")
for i, s in enumerate(rs.get_editor_property("stages")):
    log.append("  stage %d max=%s acc=%s turn=%s brake=%s ramp=%s/%s" % (i + 1, s.max_speed, s.acceleration, s.turning_strength, s.release_brake,
                                                                     s.accel_ramp_frames, s.ramp_start_fraction))
for k in ("tight_turn_cos", "tight_turn_drop_stages", "forward_stick_cos"):
    log.append("  %s = %s" % (k, rs.get_editor_property(k)))
sk = rs.get_editor_property("skid")
log.append("  skid = %s" % str(sk)[:1500])
aw = prof.get_editor_property("analog_walk")
log.append("  analog_walk = %s" % str(aw)[:1500])
for k in ("speed_properties", "acceleration_properties", "turning_properties", "brake_properties"):
    log.append("  %s = %s" % (k, list(prof.get_editor_property(k))))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(log))
print("REVDUMP: %d lines -> %s" % (len(log), OUT))
