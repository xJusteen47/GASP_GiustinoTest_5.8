"""Builds sheets/ganon_ftilt_review.html: Ganondorf forward tilt v1 review (same styles as the jab page)."""
import os, html, json
import legibility_page as P

HERE = os.path.dirname(os.path.abspath(__file__))
SHEETS = os.path.join(HERE, "sheets")
esc = lambda s: html.escape(s, quote=True)
D = "legib_ganon_v1"

def fig(src, alt, cap, w=1536, h=1024):
    if not os.path.exists(os.path.join(SHEETS, src)):
        return ""
    return f'<figure class="shot"><img src="{src}" alt="{esc(alt)}" loading="lazy" width="{w}" height="{h}"><figcaption>{esc(cap)}</figcaption></figure>'

def build():
    poses = [("f00_guard", "Guard (frame 0)"), ("f06_chamber", "Chamber (frame 6)"), ("f09_contact", "Contact (frame 9)"),
             ("f15_hold", "Hold (frame 15)"), ("f22_rechamber", "Re-chamber (frame 22)"), ("f27_land", "Landing (frame 27)"),
             ("f37_FAF", "FAF (frame 37)")]
    six = "".join(f"<h3>{esc(t)}</h3>" + fig(f"{D}/ganon_v1_views_{k}.png", f"Ganondorf ftilt {t}: front, right, back, left, top, bottom", "front · right · back / left · top · bottom") for k, t in poses)
    game = "".join(fig(f"{D}/ganon_v1_f09_contact_{v}.png", f"Contact, {c}", c, 1536, 876) for v, c in
                   (("f180", "Follow camera behind (650 cm, 25° up)"), ("f225", "Follow camera, behind right"), ("f135", "Follow camera, behind left"),
                    ("p090", "PvP side camera"), ("az225", "Low 3/4 rear (diagnostic): the foot sits behind the rear fist")))
    hold = "".join(fig(f"{D}/ganon_v1_f15_hold_{v}.png", f"Hold, {v}", f"Hold, {v}", 1536, 876) for v in ("f180", "p090"))
    elbows = "".join(fig(f"{D}/ganon_v1_elbows_f{n:02d}.png", f"Elbows frame {n}", f"Frame {n}: lead elbow (top row), rear elbow (bottom row)", 2304, 768) for n in (9, 15, 22))
    traj = "".join(fig(f"{D}/ganon_v1_traj_f15_{v}.png", f"Motion paths {v}", c, 768, 832) for v, c in
                   (("p090", "PvP side"), ("az270", "Right side"), ("az315", "3/4 front"), ("f180", "Follow camera behind")))
    vids = "".join(f'<figure class="shot"><video src="{v}" controls loop muted playsinline preload="metadata" width="512" height="512"></video><figcaption>{c}</figcaption></figure>'
                   for v, c in (("ganon_v1_slow.mp4", "Slowed"), ("ganon_v1.mp4", "60 fps")) if os.path.exists(os.path.join(SHEETS, v)))
    tim = []
    p = os.path.join(SHEETS, "ganon_v1_timings.tsv")
    if os.path.exists(p):
        rows = [l.split("\t") for l in open(p).read().splitlines() if l.strip()]
        last = rows[-1][0]
        tim = [(r[1], float(r[2])) for r in rows if r[0] == last]
    tim_rows = "".join(f"<tr><th scope='row'>{esc(s)}</th><td class='mono'>{v:.1f} s</td></tr>" for s, v in tim)
    checks = [("Clearance (self-penetration)", "≥ 2 cm", "pass: 2.0 cm minimum"),
              ("Impact spacing (ball of the foot)", "≥ 40% of the path in the last 2 frames, speed peak into contact", "pass: 84%, peak on 8→9"),
              ("Hold (tilt: ≥ 3 frames)", "≥ 60% extended, < 6% of reach per frame", "pass: 10 frames"),
              ("Weight into the kick", "from the wind-up, ≥ the jab's limit", "pass: loads back 24 cm, drives +8 cm"),
              ("Overlap", "chains peak one after another", "pass: hip→knee→ankle, hips→chest 1.2, chest→head 1.8"),
              ("Arcs and trajectories", "no zig-zag or wobble on arcs", "pass"),
              ("Joints (knees, ankles, feet, wrists, elbows)", "natural, or exaggeration along the move", "pass; support foot pivots with the hips (accepted)"),
              ("Foot readable from the game cameras (contact)", "Follow/rail rings mean ≥ 50%, each yaw ≥ 30%; PvP ≥ 60%", "pass: behind 53%, rail 71%+, PvP 92–100%"),
              ("Arms over the foot (strike and hold)", "≤ 15%", "pass")]
    crow = "".join(f'<tr><th scope="row">{esc(a)}</th><td class="mono">{esc(b)}</td><td class="ok">{esc(c)}</td></tr>' for a, b, c in checks)
    return f"""<title>Ganondorf Forward Tilt</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@500;700&family=Source+Sans+3:wght@400;600&family=JetBrains+Mono:wght@400;600&display=swap">
<style>{P.CSS}</style>
<main>
<header>
  <h1>Ganondorf Forward Tilt</h1>
  <p class="lede">Pilot v1, built from scratch on our rig from the owner's jab guard (no blockout). Reference: SSBU Ganondorf forward tilt (startup 10, active 10–12, total 39): a lead-leg push kick. Anim frames at 60 fps: chamber 6, snap 8→9, active 9–11, hold to 19, landing 27, FAF 37, guard at 43. Files: <span class="mono">ganon_ftilt_v1.casc</span>, <span class="mono">/Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V1</span>.</p>
  <p><span class="verdict ok8">v1: every check PASS</span></p>
</header>
<nav aria-label="Sections"><a href="#checks">Checks</a><a href="#video">Video</a><a href="#poses">Six views</a><a href="#game">Game cameras</a><a href="#elbows">Elbows</a><a href="#paths">Paths</a><a href="#method">Keys and tools</a><a href="#cost">Time</a></nav>
<section id="checks"><h2>Checks</h2>
<div class="tablewrap"><table><thead><tr><th>Check</th><th>Limit</th><th>v1</th></tr></thead><tbody>{crow}</tbody></table></div>
<div class="notes">
<div class="note"><h3>3D reads</h3><p>Kick line 20° outward to the lead side, torso leaning back 20° and toward the rear side 10°, hips turned 22° toward profile. From the Follow camera behind, the foot shows beside the head; from the sides the whole leg is out of the body.</p></div>
<div class="note"><h3>Rules adapted for kicks (owner to review)</h3><p>The attacking part is the foot, both arms count as hiders, weight is measured from the wind-up, the wind-up reversal is a beat, a hanging arm's elbow may point back, and the arms-over-the-foot window ends 1 frame after the hold (the re-chamber passes the hip). The jab v11 still passes everything.</p></div>
<div class="note"><h3>Known</h3><p>From a low 3/4 rear view (diagnostic, not a game camera) the foot sits behind the rear fist at contact. At 0.25x, Unreal arcs the foot up to 11 cm around the knee inside the snap frame: that is the knee extending.</p></div>
</div>
</section>
<section id="video"><h2>Video</h2><div class="vids">{vids}</div></section>
<section id="poses"><h2>Key poses, six views</h2>{six}</section>
<section id="game"><h2>Game cameras at contact and hold</h2>{game}{hold}</section>
<section id="elbows"><h2>Elbow close-ups</h2>{elbows}</section>
<section id="paths"><h2>Motion paths</h2>{traj}</section>
<section id="method"><h2>Keys and tools</h2>
<div class="notes">
<div class="note"><h3>Sparse keys in Cascadeur</h3><p>Tried first: 13 poses per layer, clamped/Bezier/linear, layers offset for overlap. The rig's IK chains span layers (shoulder, elbow and fist on three layers; hip, knee and ankle on three), and Cascadeur interpolates each IK point on its own in world space, so offset layers zig-zag the joint between them. v1 keeps the overlap in the curves and keys every frame on every layer: exactly what Unreal receives (the 60 fps bake).</p></div>
<div class="note"><h3>Unbaking</h3><p>Cascadeur's "Animation unbaking" turns v1 into 13 keys per layer (same frames on all layers, clamped Bezier), within 3 cm everywhere except the snap: it smooths the 1-frame snap away (foot 35 cm early at frame 8, speed peak off the contact frame). Re-adding the frame-8 key still leaves 22 cm at frame 7. Usable as an editable base only with the strike frames (6–9) re-keyed by script.</p></div>
<div class="note"><h3>AutoPhysics (side test)</h3><p>Same v1 snapped to Cascadeur's AutoPhysics (support foot as fulcrum, kicking foot free in the air, secondary on the hands, priority frames 0, 9, 11, 37, 43). Worse than v1: the lead forearm goes 7 cm into the torso on frames 17–21, the forward weight drive drops from +8 to −0.3 cm, the lead elbow wobbles in the recovery. Spacing, hold and legibility unchanged. v1 stays.</p></div>
<div class="note"><h3>Tangents</h3><p>Cascadeur: trajectory tangent handles and the graph editor have no script API (UI drags only, imprecise). Unreal 5.8 Sequencer: per-key arrive/leave tangents and weights are settable from Python with exact readback (tested on a pilot Level Sequence). Next step if wanted: bake to a Control Rig track, shape the spacing there, bake back.</p></div>
</div>
{fig('ganon_v1_vs_autophys_f20.png', 'Frame 20: v1 above, AutoPhysics below (front, right, back)', 'Frame 20, top: v1 (hand-placed); bottom: AutoPhysics (lead forearm into the torso, weight back over the support foot)', 1536, 1024)}
</section>
<section id="cost"><h2>Time per step (last pass)</h2>
<div class="tablewrap"><table><thead><tr><th>Step</th><th>Wall clock</th></tr></thead><tbody>{tim_rows}</tbody></table></div>
<p class="lede">A full Cascadeur → Unreal → checks pass takes about 16 s plus the legibility ray-cast (about 130 s, Python, not Unreal). Six views of one pose render in about 5 s. The real cost is the reasoning between passes (about 40 passes for v1, most of them in the sparse-key attempt).</p>
</section>
</main>
"""

if __name__ == "__main__":
    out = os.path.join(SHEETS, "ganon_ftilt_review.html")
    open(out, "w", encoding="utf-8").write(build())
    print(out)
