"""Builds sheets/ganon_v2_review.html: Ganondorf forward tilt v2 review, v1 vs v2 (same styles as the jab and v1 pages)."""
import os, html
import legibility_page as P

HERE = os.path.dirname(os.path.abspath(__file__))
SHEETS = os.path.join(HERE, "sheets")
esc = lambda s: html.escape(s, quote=True)
D2, D1 = "legib_ganon_v2", "legib_ganon_v1"

def fig(src, alt, cap, w=1536, h=1024):
    if not os.path.exists(os.path.join(SHEETS, src)):
        return ""
    return f'<figure class="shot"><img src="{src}" alt="{esc(alt)}" loading="lazy" width="{w}" height="{h}"><figcaption>{esc(cap)}</figcaption></figure>'

# files the page references (for the Artifact publish)
def files():
    out = []
    for root, _, fs in os.walk(SHEETS):
        pass
    import re
    txt = build()
    for m in re.finditer(r'(?:src)="([^"]+)"', txt):
        if os.path.exists(os.path.join(SHEETS, m.group(1))):
            out.append(m.group(1))
    return sorted(set(out))

def build():
    poses = [("f00_load", "Load (frame 0)"), ("f03_weight", "Weight on the back foot (frame 3)"), ("f06_liftoff", "Lift-off (frame 6)"),
             ("f08_presnap", "Before the snap (frame 8)"), ("f09_contact", "Contact (frame 9)"), ("f15_hold", "Hold (frame 15)"),
             ("f20_rechamber", "Re-chamber (frame 20)"), ("f23_retract", "Retract (frame 23)"), ("f27_land", "Landing (frame 27)"),
             ("f32_rise", "Rising (frame 32)"), ("f37_faf", "FAF (frame 37)")]
    six = "".join(f"<h3>{esc(t)}</h3>" + fig(f"{D2}/ganon_v2_views_{k}.png", f"Ganondorf ftilt v2 {t}: front, right, back, left, top, bottom", "front · right · back / left · top · bottom") for k, t in poses)
    vs = "".join(f"<h3>{esc(t)}</h3>" + fig(f"{D1}/ganon_v1_views_{a}.png", f"v1 {t}", f"v1 — {t}") + fig(f"{D2}/ganon_v2_views_{b}.png", f"v2 {t}", f"v2 — {t}")
                 for a, b, t in (("f09_contact", "f09_contact", "Contact"), ("f15_hold", "f15_hold", "Hold")))
    game = "".join(fig(f"{D2}/ganon_v2_f09_contact_{v}.png", f"Contact, {c}", c, 1536, 876) for v, c in
                   (("f180", "Follow camera behind (650 cm, 25° up)"), ("f225", "Follow camera, behind right"), ("f135", "Follow camera, behind left"), ("p090", "PvP side camera")))
    hold = "".join(fig(f"{D2}/ganon_v2_f15_hold_{v}.png", f"Hold, {v}", f"Hold, {v}", 1536, 876) for v in ("f180", "p090"))
    elbows = "".join(fig(f"{D2}/ganon_v2_elbows_f{n:02d}.png", f"Elbows frame {n}", f"Frame {n}: lead elbow (top row), rear elbow (bottom row)", 2304, 768) for n in (6, 9, 15, 25, 30))
    traj = "".join(fig(f"{D2}/ganon_v2_traj_f15_{v}.png", f"Motion paths {v}", c, 768, 832) for v, c in
                   (("p090", "PvP side"), ("az270", "Right side"), ("az315", "3/4 front"), ("f180", "Follow camera behind")))
    strips = fig("ganon_v1_strip.png", "v1 every frame", "v1, every frame (0–43)", 2560, 1280) + fig("ganon_v2_strip.png", "v2 every frame", "v2, every frame (0–43)", 2560, 1280)
    sil = fig("ganon_v2_silhouettes.png", "v2 silhouettes", "v2 silhouettes, frames 0, 3, 9, 15, 22, 27, 37 from eight sides", 2108, 1822)
    vids = "".join(f'<figure class="shot"><video src="{v}" controls loop muted playsinline preload="metadata" width="512" height="512"></video><figcaption>{c}</figcaption></figure>'
                   for v, c in (("ganon_v2_slow.mp4", "v2 slowed"), ("ganon_v2.mp4", "v2, 60 fps"), ("ganon_v1.mp4", "v1, 60 fps")) if os.path.exists(os.path.join(SHEETS, v)))
    checks = [("Pre-flight (model points)", "0 flags", "0 flags", "—"),
              ("Balance: centre of mass inside the feet (2 cm)", "every frame", "pass: worst 0.4 cm inside the limit (frame 6)", "fail: 3–25 cm out, frames 2–26"),
              ("Weight over the support foot before lift-off", "before the toes leave", "pass: 3.2 cm inside at frame 2", "fail"),
              ("Support knee on one foot", "10–45°", "pass", "fail: 89–99° (near-kneel)"),
              ("Hip drive into the kick / counter-lean", "≥ 8 cm / ≥ 2 cm", "pass: +15.6 cm / −8.7 cm", "+8 cm / —"),
              ("Torso lean at contact", "—", "29° back (guard −4°)", "20°"),
              ("Impact spacing (ball of the foot)", "≥ 40% in the last 2 frames, peak into contact", "pass: 76%, peak on 8→9", "84%"),
              ("Hold (tilt ≥ 3 frames) with recoil", "", "pass: 11 frames, recoil 7.3 cm", "10 frames"),
              ("Arcs, overlap, joints, elbows, wrists", "", "pass", "pass"),
              ("Recovery spacing", "fast out, slow in", "pass: peak at 21, last step 0.12× the largest", "—"),
              ("Clearance (self-penetration)", "≥ 2 cm", "pass: 5.5 cm minimum (lead forearm–spine, frame 8)", "2.0 cm"),
              ("Legibility (foot from the game cameras, rear elbow)", "", "pass: Follow behind 47–51%, rail 68–71%, PvP 93–100%", "pass")]
    crow = "".join(f'<tr><th scope="row">{esc(a)}</th><td class="mono">{esc(b)}</td><td class="ok">{esc(c)}</td><td class="mono">{esc(d)}</td></tr>' for a, b, c, d in checks)
    return f"""<title>Ganondorf Forward Tilt v2</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@500;700&family=Source+Sans+3:wght@400;600&family=JetBrains+Mono:wght@400;600&display=swap">
<style>{P.CSS}</style>
<main>
<header>
  <h1>Ganondorf Forward Tilt v2</h1>
  <p class="lede">v2 rebuilds the lead-leg push kick from the upright standing idle so it is balanced and heavy. v1 was "unbalanced and weightless": a near-kneel on one leg with the centre of mass up to 25 cm off the foot. Timing is unchanged (SSBU: startup 10, active 10–12, total 39; anim: snap 8→9, hold to 19, foot down 27, FAF 37).</p>
  <p><span class="verdict ok8">v2: every check PASS · by-eye review PASS with open points</span></p>
</header>
<nav aria-label="Sections"><a href="#checks">Checks</a><a href="#review">By-eye review</a><a href="#video">Video</a><a href="#vs">v1 vs v2</a><a href="#poses">Six views</a><a href="#game">Game cameras</a><a href="#elbows">Elbows</a><a href="#paths">Paths</a><a href="#changes">Changes</a></nav>
<section id="checks"><h2>Checks</h2>
<div class="tablewrap"><table><thead><tr><th>Check</th><th>Limit</th><th>v2</th><th>v1</th></tr></thead><tbody>{crow}</tbody></table></div>
</section>
<section id="review"><h2>By-eye review</h2>
<div class="notes">
<div class="note"><h3>Weight</h3><p>He pushes into the kick: the hips travel 15.6 cm forward from the wind-up while the chest leans back against them, and the support knee stays bent under the hips. In the side view of the hold the centre of mass drops onto the support foot, between heel and ball. He does not fall back.</p></div>
<div class="note"><h3>Line of action</h3><p>Contact and hold read as one long diagonal from the support foot up through the leaning torso, with the kicking leg level at belly height: a clear C, not a pole. The rear arm hangs back and down as the counterweight.</p></div>
<div class="note"><h3>Story per phase</h3><p>Idle → the weight goes onto the back foot and the front heel lifts (0–5) → the knee chambers (6–8) → 1-frame snap (8→9) → held push with recoil (9–19) → re-chamber and the foot comes down (20–27) → the body rises back to the idle by the FAF (37). Every phase reads in the strip.</p></div>
<div class="note"><h3>Open points</h3><p>1. The load (0–5) is readable but small: frame 0 is close to the idle, with the front leg forward and the heel up. A deeper sink there would make the anticipation heavier. 2. In the hold the rear arm stays straight out for 11 frames: correct as a counterweight, slightly stiff. 3. At the landing (27) both arms open wide: it reads as catching balance. 4. Hip, thigh, shin and foot peak in the same frame, because the snap is 1 frame. 5. Tangencies: the hands line up with the thighs at the idle frames (0, 37: the idle itself) and at frame 28.</p></div>
</div>
</section>
<section id="video"><h2>Video</h2><div class="vids">{vids}</div></section>
<section id="vs"><h2>v1 vs v2</h2>{strips}{vs}</section>
<section id="poses"><h2>Key poses, six views</h2>{six}{sil}</section>
<section id="game"><h2>Game cameras at contact and hold</h2>{game}{hold}</section>
<section id="elbows"><h2>Elbow close-ups</h2>{elbows}</section>
<section id="paths"><h2>Motion paths</h2>{traj}</section>
<section id="changes"><h2>Changes in this pass</h2>
<div class="notes">
<div class="note"><h3>Pipeline bugs (fixed)</h3><p>The first v2 runs checked a stale clip. After the repo move, Cascadeur scripts read their data from Cascadeur's install folder, so the keys never landed, and the driver hid the error. The fist step had never run under this driver, and the arm check read a quaternion sign flip as a reversed elbow. All three are fixed; jab v11 and v1 give byte-identical results.</p></div>
<div class="note"><h3>Model edits</h3><p>Front heel lift lowered (ankle 141–148° &gt; 140°). Rear elbow points back-down in the hold, not straight down. The recovery keys are re-timed: rear arm 30/32, lead arm keys at 26/30 softened, a lead elbow pole that keeps pointing back, and a foot key at 25 that rounds the drop.</p></div>
</div>
</section>
</main>
"""

if __name__ == "__main__":
    out = os.path.join(SHEETS, "ganon_v2_review.html")
    open(out, "w", encoding="utf-8").write(build())
    print(out)
    print(" ".join(files()))
