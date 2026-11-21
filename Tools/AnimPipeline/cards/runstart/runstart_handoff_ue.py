"""Run start hand-off fix (runstart-phase-desktop, 2026-11-21), systemic for every run start of ours, switchable and reversible.
Review verdicts v4r8 a/b/c (claude/project-thread-3lh042 66877cf6): the rev-up is hidden at entry and the exit into the run loop
is out of phase. Causes, from the v8c captures (frames.jsonl, clip/weight/time per frame) and the clip curves:
  ENTRY  a stock walk start plays ~6 frames first; the run start row is then pose matched and entered at ~0.13 s (rev-up skipped)
         and blended in over 0.3 s QuadraticInOut (weight 0.15 at 0.13 s, 0.78 at 0.23 s).
  EXIT   the loop row's PoseSearchColumn uses PSS_Relaxed_Loops = Trajectory + Curve channel `Phase` (t, t-0.033) + Heading
         (Saved/Routing/pss_channels.txt): the phase curve is the ONLY foot information. Our clips carry `phase` in our 0..1 sawtooth
         (0 = L contact, 0.5 = R), the loop the stock triangle (+1 at L contact onset, -1 at R onset), so the match lands on the other
         foot (a: R just planted, loop entered ~0.03 s before its L plant) and the 0.5 s crossfade averages two feet.
Fix, stock knobs only (Unreal Assistant: Saved/Assistant/replies_runstart_phase.json; divergence row 19):
  1. clip COPY <src>h per start (sources never touched; v4r7 stays locked): `phase` rewritten in the stock triangle from the clip's
     own contact_l/contact_r onsets (the convention the loop schema reads). Nothing else in the clip changes (pose, root, timing).
  2. sibling rows of every enabled source row in "Run F Loco Transitions", variant range k + 10 (cvar gstn.RunStart.Handoff 1 adds
     10 to gstn.RunStart.Variant in UGSTN_LocoBaseSubsystem): OUT BlendTime RSH_BLEND (0.05 s = 3 frames at 60 fps, owner 2026-11-21: the Luigi/Mario reference
     enters the start almost instantly; the Unreal Assistant suggested 0.15-0.2) with RSH_CURVE (CircularOut), profile
     kept (FastFeet_FastRoot); PoseSearchColumn SamplingRange 0..RSH_ENTRY_MAX (0.02 s) so the start enters on its first frame.
Env RSH_BLEND, RSH_CURVE, RSH_ENTRY_MAX, RSH_SRC "k=/Game/...;k=..." (default the 4 run starts). Dry run unless GSTN_SAVE=1 or
CLIPFIX_WRITE=1 (Tools/CI/Save-Assets.ps1). Report Saved/AnimPipeline/rows/runstart_handoff.txt; prints max 3 lines."""
import os
import re
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
LEAF = "Run F Loco Transitions"
VAR = "GSTN_RunStartVariant"
D = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_"
SRC = {}
for part in os.environ.get("RSH_SRC", "0=%sv4r7;1=%sv4r8c;2=%sv4r8a;3=%sv4r8b" % (D, D, D, D)).split(";"):
    k, p = part.split("=", 1)
    SRC[p.strip()] = int(k)
OFFSET = 10
BLEND = float(os.environ.get("RSH_BLEND", "0.05"))
CURVE = os.environ.get("RSH_CURVE", "CircularOut")
ENTRY_MAX = float(os.environ.get("RSH_ENTRY_MAX", "0.02"))
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_handoff.txt")
FLT = unreal.RawCurveTrackTypes.RCT_FLOAT
log = []


def call(name, *args):
    return TS.call_method(name, tuple(args))


def onsets(times, vals):
    """Times of contact onsets (0 -> 1) after a lift; a foot already down at the first key is not a step."""
    out, prev = [], vals[0] > 0.5
    for t, v in zip(times, vals):
        on = v > 0.5
        if on and not prev:
            out.append(t)
        prev = on
    return out


def stock_phase(times, cl, cr):
    """Stock GASP triangle (as curves_author.phase_stock, validated on M_Relaxed_Run_Loop_F / Start_F_L/R): +1 at each L onset,
    -1 at each R onset, linear between; before the first onset a ramp from the opposite value; after the last one the last slope,
    clamped to +-1. Two onsets of the same foot in a row (a re-plant glitch in the last keys): the first one counts."""
    ev = sorted([(t, 1.0) for t in onsets(times, cl)] + [(t, -1.0) for t in onsets(times, cr)])
    ev = [e for i, e in enumerate(ev) if i == 0 or e[1] != ev[i - 1][1]]
    if len(ev) < 2:
        return None, ev
    out = []
    for t in times:
        if t <= ev[0][0]:
            out.append(-ev[0][1] + 2.0 * ev[0][1] * (t / max(ev[0][0], 1e-3)))
        elif t >= ev[-1][0]:
            span = max(ev[-1][0] - ev[-2][0], 1e-3)
            out.append(max(-1.0, min(1.0, ev[-1][1] - 2.0 * ev[-1][1] * (t - ev[-1][0]) / span)))
        else:
            for (ta, pa), (tb, pb) in zip(ev, ev[1:]):
                if ta <= t <= tb:
                    out.append(pa + (pb - pa) * (t - ta) / max(tb - ta, 1e-6))
                    break
    return out, ev


# 1. clip copies with the stock phase
DST = {}
for src in SRC:
    dst = src + "h"
    DST[src] = dst
    if EAL.does_asset_exist(dst):
        seq = unreal.load_asset(dst)
    elif SAVE:
        assert EAL.duplicate_asset(src, dst), "duplicate failed " + dst
        seq = unreal.load_asset(dst)
    else:
        log.append("%s: would duplicate to %s" % (src.split("/")[-1], dst.split("/")[-1]))
        seq = unreal.load_asset(src)
    t_l, cl = AL.get_float_keys(seq, "contact_l")
    t_r, cr = AL.get_float_keys(seq, "contact_r")
    assert [round(x, 4) for x in t_l] == [round(x, 4) for x in t_r], "contact keys differ in " + src
    ph, ev = stock_phase(list(t_l), list(cl), list(cr))
    assert ph and len(ph) == len(t_l), "phase not built for " + src
    log.append("%s: onsets %s" % (dst.split("/")[-1], " ".join("%s%.3f" % ("L" if p > 0 else "R", t) for t, p in ev)))
    if SAVE:
        if AL.does_curve_exist(seq, "phase", FLT):
            AL.remove_curve(seq, "phase", False)
        AL.add_curve(seq, "phase", FLT, False)
        AL.add_float_curve_keys(seq, "phase", list(t_l), ph)
        log.append("  saved clip %s" % EAL.save_asset(dst, only_if_is_dirty=False))


# 2. sibling rows
def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


def asset_of(row):
    a = row.asset
    return a.get_path_name().split(".")[0] if a else ""


def rng(k):
    return "(Min=%.6f,Max=%.6f,bNoMin=False,bNoMax=False)" % (k - 0.5, k + 0.5)


leaf = [t for t in tables(unreal.load_asset(CHT), []) if t.get_name() == LEAF][0]
info = call("GetChooserTable", leaf)
types = [str(c.type) for c in info.columns]
c_var = next(i for i, c in enumerate(info.columns) if VAR in [str(x) for x in c.property_chain])
c_out = next(i for i, t in enumerate(types) if t.endswith("OutputStructColumn"))
c_ps = next(i for i, t in enumerate(types) if t.endswith("PoseSearchColumn"))


def tuned(col, text):
    if col == c_out:
        text = re.sub(r"(BlendTime_[0-9A-F_]+=)[0-9.]+", lambda m: m.group(1) + "%.6f" % BLEND, text)
        return re.sub(r"(BlendCurve_[0-9A-F_]+=)\w+", lambda m: m.group(1) + CURVE, text)
    if col == c_ps:
        return re.sub(r"SamplingRange=\(Min=[0-9.]+,Max=[0-9.]+\)", "SamplingRange=(Min=0.000000,Max=%.6f)" % ENTRY_MAX, text)
    return text


def filters(row):
    return [str(c) for n, c in enumerate(row.cells) if n not in (c_var, c_out, c_ps)]


if SAVE:
    rows = list(info.rows)
    srcs = [i for i, r in enumerate(rows) if asset_of(r) in SRC and not r.get_editor_property("bDisabled")]
    for i in reversed(srcs):   # bottom-up: inserting below row i never shifts a source row still to visit
        rows = list(call("GetChooserTable", leaf).rows)
        src = asset_of(rows[i]); dst = DST[src]; k = SRC[src] + OFFSET
        j = next((n for n, r in enumerate(rows) if asset_of(r) == dst and str(r.cells[c_var]) == rng(k)
                  and filters(r) == filters(rows[i])), None)
        if j is None:
            j = call("AddChooserRow", leaf, rows[i].result_type, i + 1, unreal.load_asset(dst))
            log.append("row %d %s: added sibling %s at %d" % (i, src.split("/")[-1], dst.split("/")[-1], j))
        for c, cell in enumerate(rows[i].cells):
            val = rng(k) if c == c_var else tuned(c, str(cell).replace(src + "." + src.split("/")[-1], dst + "." + dst.split("/")[-1]))
            call("SetChooserCell", leaf, c, j, val)
        call("SetChooserRowDisabled", leaf, j, False)

rows = list(call("GetChooserTable", leaf).rows)
bad = 0
for i, r in enumerate(rows):
    a = asset_of(r)
    if r.get_editor_property("bDisabled"):
        continue
    if a in DST.values():
        o, p = str(r.cells[c_out]), str(r.cells[c_ps])
        b = re.search(r"BlendTime_[0-9A-F_]+=([0-9.]+)", o).group(1)
        cv = re.search(r"BlendCurve_[0-9A-F_]+=(\w+)", o).group(1)
        sr = re.search(r"SamplingRange=\(([^)]*)\)", p).group(1)
        ok = abs(float(b) - BLEND) < 1e-4 and cv == CURVE and ("Max=%.6f" % ENTRY_MAX) in sr and a.split("/")[-1] + "'" in p
        bad += not ok
        log.append("check row %d %s var=%s blend=%s %s sampling=%s %s" % (i, a.split("/")[-1], str(r.cells[c_var]), b, cv, sr,
                                                                         "OK" if ok else "MISMATCH"))
    elif a in SRC:
        log.append("source row %d %s var=%s untouched" % (i, a.split("/")[-1], str(r.cells[c_var])))
if SAVE:
    log.append("saved chooser %s" % EAL.save_asset(CHT, False))
else:
    log.append("dry run (nothing saved)")
n = sum(x.startswith("check") for x in log)
log.append("ESITO: %s handoff rows %d mismatch %d" % ("OK" if not bad and (n or not SAVE) else "FAIL", n, bad))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:]))
