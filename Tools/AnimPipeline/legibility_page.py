"""Builds sheets/legibility_review.html: the v7 legibility review page (rules table, large shots, heatmaps, mesh check)."""
import os, html

HERE = os.path.dirname(os.path.abspath(__file__))
SHEETS = os.path.join(HERE, "sheets")

POSES = [("f01_contact", "contact", "Contact", "frame 1"), ("f04_retract", "retract", "Retract", "frame 4"),
         ("f08_spring", "spring", "Spring", "frame 8"), ("f14_exit", "exit", "Exit toward guard", "frame 14, worst case"),
         ("f17_FAF", "faf", "FAF", "frame 17")]
VIEWS = [("az315", "3/4 front, rear-arm side (the video's view)"), ("az270", "Side, rear-arm side"), ("az225", "3/4 rear"),
         ("az180", "Rear"), ("g-30", "Game camera, 30° off behind (az 150)"), ("g-15", "Game camera, 15° off behind (az 165)"),
         ("g0", "Game camera, straight behind"), ("g+15", "Game camera, 15° off behind (az 195)"), ("g+30", "Game camera, 30° off behind (az 210)")]
RULES = [
    ("Wrists in line with the forearm (new)", "curl −30…+35°, sideways ≤ 20°, no fist frozen while the forearm turns; an exaggeration along the hand's motion inside the strike is accepted",
     "fail", "fail: rear 4, 7", "fail: front 3–7 and tail, rear 1–2, 8–10", "fail: rear fist all clip (+38…+69°), front frame 3 (+86° / +77°, frozen)", "pass: +8…+17° curl, ≤ 11° sideways",
     "The owner's v8 note: rear fist bent inward, front fist fixed forward at frame 3 while the elbow goes back."),
    ("Attacking arm readable from the gameplay cameras (contact)", "each ring: mean ≥ 50%, every yaw ≥ 30% of its side-view area (front yaw excluded); PvP side ≥ 60%",
     "fail: behind 10%", "fail: behind 10%", "fail: behind 10%", "pass: behind 43.5%", "pass: behind 39%",
     "Follow camera (650 cm, 25° above) all around the character, rail ring at 40°, PvP side."),
    ("Rear arm hides the fist (while the fist is out)", "≤ 15% hidden only by the rear arm",
     "fail: 1–6, 10 from behind", "fail: 1–6 from behind", "fail: 1–3 from behind and the game camera", "pass", "pass",
     "Strict on every gameplay camera while the fist is out; on the low diagnostic views during the strike. In the recovery spring the low 3/4-rear view sees the chambered fist behind the tucked rear forearm in every pose tried (info, owner can override)."),
    ("Rear elbow away from the body (side view)", "> 10 cm outside the torso outline", "fail: 4–7, 11", "fail: 4–10", "fail: 1–2, 8–11", "pass", "pass",
     "In this view the guard is inside the outline, so only the move is flagged."),
    ("Fists closed at guard and FAF", "finger curl ≥ 180°", "fail", "fail", "fail", "pass", "pass", "180° is about 85% of the blockout's fist (209°)."),
    ("Attacking arm exaggerated out of the body (PvP side, contact)", "reach ≥ 25 cm, elbow ≥ 5 cm outside the outline", "pass", "pass", "pass", "pass", "pass",
     "The arm reaches well out of the body at contact."),
    ("Fist distinct at contact (3/4 views)", "≥ 50%", "pass", "pass", "pass", "pass", "pass", "The fist reads from every 3/4 view at contact."),
    ("Rear elbow behind the back plane", "> 0 cm", "pass", "pass", "pass", "pass", "pass",
     "Corrected: the earlier 'fail' for v5–v7 came from a bug in the chest frame (flipped when the torso turns past 90° at contact)."),
]

# v11 column for the rules above (same order), and the arm rules added after the owner's v9 note
V11 = {"Wrists in line with the forearm (new)": "pass: curl −1…+28°, sideways ≤ 17°",
       "Attacking arm readable from the gameplay cameras (contact)": "pass: behind 40.5%",
       "Rear arm hides the fist (while the fist is out)": "pass", "Rear elbow away from the body (side view)": "pass",
       "Fists closed at guard and FAF": "pass", "Attacking arm exaggerated out of the body (PvP side, contact)": "pass",
       "Fist distinct at contact (3/4 views)": "pass", "Rear elbow behind the back plane": "pass"}
ARM_RULES = [
    ("Elbows bend naturally (new, owner's v9 note)", "a bent elbow points down (pole up ≤ −0.3), out ≤ 0.6, upper arm raised ≤ 50°, no reverse bend; accepted only along the strike in the action frames",
     "fail: rear arm all clip (out, raised, wrong way 1–13)", "fail: rear arm all clip (out, raised, wrong way 1–13)", "fail: front elbow up 5–25, rear out all clip",
     "fail: front elbow raised and out 4–10 (55–66°)", "fail: front elbow raised and out 4–10 (55–65°)", "pass: front raised 31–36°, pointing down",
     "The owner's v9 note: left elbow toward the outside in the chamber. It came from my v8 chamber bend (1.6 outward), not from the blockout."),
    ("Forearm and hand roll (new)", "≤ 120° from the guard, ≤ 60° in one frame (≤ 100° right after contact); the corkscrew at contact is accepted",
     "pass", "fail: rear jump at 11", "fail: rear jump at 1", "pass", "pass", "pass",
     "Twist the skin can't follow (candy-wrapper). Measured on the bones; Unreal's Python can't read the posed skeletal mesh's vertices."),
]

CSS = """
/* Layout: one reading column for the summary, full-width figures for the large shots, heatmaps side by side last. */
:root {
  --bg: #f4f4f6; --panel: #ffffff; --fg: #1d1f24; --muted: #5d6170; --line: #d9dbe2;
  --fist: #c8102e; --ok: #1f7a3d; --okbg: #e3f2e7; --badbg: #fbe5e8;
  --display: "Barlow Semi Condensed", "Arial Narrow", system-ui, sans-serif;
  --body: "Source Sans 3", "Segoe UI", system-ui, sans-serif;
  --mono: "JetBrains Mono", Consolas, ui-monospace, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #15161a; --panel: #1e2026; --fg: #e8e9ee; --muted: #a2a6b4; --line: #343743;
  --fist: #ff5a6e; --ok: #6fd18f; --okbg: #183223; --badbg: #3a1a20; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #15161a; --panel: #1e2026; --fg: #e8e9ee; --muted: #a2a6b4; --line: #343743;
  --fist: #ff5a6e; --ok: #6fd18f; --okbg: #183223; --badbg: #3a1a20; color-scheme: dark }
body { background: var(--bg); color: var(--fg); font: 16px/1.55 var(--body); }
main { max-width: 1600px; margin: 0 auto; padding-block: 28px 64px; padding-left: max(16px, env(safe-area-inset-left, 0px));
       padding-right: max(16px, env(safe-area-inset-right, 0px)); display: grid; gap: 36px; }
h1, h2, h3 { font-family: var(--display); text-wrap: balance; margin: 0; line-height: 1.15; }
h1 { font-size: clamp(30px, 4vw, 44px); }
h2 { font-size: 26px; border-bottom: 2px solid var(--line); padding-bottom: 6px; }
h3 { font-size: 22px; }
.fr { font: 600 14px var(--mono); color: var(--muted); margin-left: 8px; }
.lede { max-width: 70ch; color: var(--muted); margin: 8px 0 0; }
.verdict { display: inline-block; font: 700 15px var(--mono); letter-spacing: .04em; background: var(--badbg); color: var(--fist);
           padding: 6px 12px; border-radius: 4px; }
.tablewrap { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; min-width: 860px; background: var(--panel); font-size: 15px; }
th, td { border: 1px solid var(--line); padding: 8px 10px; text-align: left; vertical-align: top; }
thead th { font: 600 13px var(--body); text-transform: uppercase; letter-spacing: .06em; color: var(--muted); }
td.mono { font-family: var(--mono); white-space: nowrap; }
td.ok { background: var(--okbg); color: var(--ok); font-weight: 600; }
td.bad { background: var(--badbg); }
td.why { color: var(--muted); max-width: 46ch; }
.notes { display: grid; gap: 14px; grid-template-columns: repeat(auto-fit, minmax(min(100%, 340px), 1fr)); }
.note { background: var(--panel); border: 1px solid var(--line); border-radius: 6px; padding: 14px 16px; min-width: 0; }
.note h3 { font-size: 19px; margin-bottom: 6px; }
.note p { margin: 0; }
.pose { display: grid; gap: 14px; }
.shot { margin: 0; display: grid; gap: 4px; }
.shot img { width: 100%; height: auto; display: block; border-radius: 4px; background: #181818; }
.shot figcaption, .cap { font: 600 14px var(--mono); color: var(--muted); }
nav { display: flex; flex-wrap: wrap; gap: 8px 16px; font: 600 15px var(--body); }
nav a { color: var(--fg); }
a:focus-visible { outline: 2px solid var(--fist); outline-offset: 2px; }
.heat { display: grid; gap: 18px; grid-template-columns: repeat(auto-fit, minmax(min(100%, 420px), 1fr)); }
.heat figure { margin: 0; min-width: 0; }
.heat img { width: 100%; height: auto; background: #fff; border-radius: 4px; }
.spot { display: grid; gap: 16px; grid-template-columns: minmax(0, 360px) minmax(0, 1fr); align-items: start; }
.spot img { width: 100%; height: auto; image-rendering: pixelated; border-radius: 4px; }
.spot p { margin: 0 0 8px; max-width: 70ch; }
@media (max-width: 640px) { .spot { grid-template-columns: 1fr; } }
.verdict.ok8 { background: var(--okbg); color: var(--ok); }
.pair { display: grid; gap: 12px; grid-template-columns: repeat(auto-fit, minmax(min(100%, 520px), 1fr)); }
.pair figure { min-width: 0; }
.vids { display: grid; gap: 12px; grid-template-columns: repeat(auto-fit, minmax(min(100%, 260px), 1fr)); }
.vids video { width: 100%; height: auto; background: #000; border-radius: 4px; }
"""

def esc(s):
    return html.escape(s, quote=True)

def cell(x):
    return f'<td class="{"ok" if x.startswith("pass") else "bad"}">{esc(x)}</td>'


def v11_vs_v9():
    import json
    A = json.load(open(os.path.join(SHEETS, "arm_rows_v9_v11.json")))
    fr = (0, 1, 3, 4, 5, 6, 8, 10, 12, 15, 18)
    def row(t):
        l = A[t]["l"]
        r1 = "".join(f'<td class="{"bad" if l[f]["elevation"] > 50 and l[f]["flex"] > 30 else ""}">{l[f]["elevation"]:.0f}°</td>' for f in fr)
        r2 = "".join(f'<td class="{"bad" if l[f]["pole_up"] > -0.3 and l[f]["flex"] > 30 else ""}">{-l[f]["pole_up"]:.2f}</td>' for f in fr)
        return f"<tr><th scope='row'>{t}: upper arm raised</th>{r1}</tr><tr><th scope='row'>{t}: elbow points down (1 = straight down)</th>{r2}</tr>"
    table = ("<div class='tablewrap'><table><thead><tr><th>Front (left) arm</th>" + "".join(f"<th>f{f}</th>" for f in fr) + "</tr></thead><tbody>"
             + row("v9") + row("v11") + "</tbody></table></div>")
    poses = [("f00_guard", "Guard"), ("f01_contact", "Contact"), ("f04_chamber", "Frame 4, the chamber (where v9's elbow went out)"),
             ("f08_spring", "Spring"), ("f12_return", "Return"), ("f18_FAF", "FAF (anim 18)")]
    rows = []
    for key, title in poses:
        figs = []
        for t in ("v9", "v11"):
            f = f"legib_{t}/{t}_views_{key}.png"
            if os.path.exists(os.path.join(SHEETS, f)):
                figs.append(f'<figure class="shot"><img src="{f}" alt="{t}, {esc(title)}: front, right, back, left, top, bottom" loading="lazy" width="1536" height="1024"><figcaption>{t}: front · right · back / left · top · bottom</figcaption></figure>')
        rows.append(f'<h3>{esc(title)}</h3>' + "".join(figs))
    elbows = []
    for n in (4, 8, 1, 18):
        figs = "".join(f'<figure class="shot"><img src="legib_{t}/{t}_elbows_f{n:02d}.png" alt="{t} elbows frame {n}, six views" loading="lazy" width="2304" height="768"><figcaption>{t}: front elbow (top row), rear elbow (bottom row); front · right · back · left · top · bottom</figcaption></figure>'
                       for t in ("v9", "v11") if os.path.exists(os.path.join(SHEETS, f"legib_{t}/{t}_elbows_f{n:02d}.png")))
        elbows.append(f"<h3>Elbow close-ups, frame {n}</h3>{figs}")
    vids = "".join(f'<figure class="shot"><video src="{v}" controls loop muted playsinline preload="metadata" width="512" height="512"></video><figcaption>{c}</figcaption></figure>'
                   for v, c in (("v9_slow.mp4", "v9, slowed"), ("v11_slow.mp4", "v11, slowed"), ("v9.mp4", "v9, 60 fps"), ("v11.mp4", "v11, 60 fps"))
                   if os.path.exists(os.path.join(SHEETS, v)))
    return f"""<section id="v11"><h2>v11 against v9</h2>
<p class="lede">The owner's v9 note: the front (left) elbow went out to the side. Where it came from: on frames 4–10 (the chamber after the hit) the upper arm was raised 55–65° and the elbow pointed sideways instead of down. The blockout and v7 kept it lower and pointing down. It was my v8 change (the chamber bend pushed 1.6 outward), carried into v9. The guard frames were fine. Why I missed it: no check measured where the elbow points, the legibility rule rewards an elbow out at contact, and in the v9 review I looked at the wrists. An arm check now runs inside the motion check: elbow direction, how far the upper arm is raised, reverse bend, forearm and hand roll, roll jumps. It fails v8 and v9 on frames 4–10 and passes v11.</p>
<p class="lede">v11 = v9 with the owner's guard: stance 8 cm wider per foot, 6 cm longer and 9 cm lower, knees over the toes; upper body turned 15° more toward profile, face kept on the opponent; front fist in front of the shoulder at chest height, elbow down; rear fist by the chin, elbow down on the ribs. The chamber bends the elbow down. At contact the rear hand sits 7 cm further forward, so its forearm clears the chest. Same timing, contact, hold, wrists and FAF (anim 18). Clearance ≥ 2.5 cm (v9: 2.7). Every check passes.</p>
<div class="vids">{vids}</div>
{table}
{"".join(rows)}
{"".join(elbows)}
</section>"""

def v9_vs_v8():
    poses = [("f00_guard", "Guard"), ("f01_contact", "Contact"), ("f03_retract", "Frame 3, the retraction (owner's note)"), ("f08_spring", "Spring"), ("f18_FAF", "FAF (anim 18)")]
    rows = []
    for key, title in poses:
        figs = []
        for t in ("v8", "v9"):
            f = f"legib_{t}/{t}_views_{key}.png"
            if os.path.exists(os.path.join(SHEETS, f)):
                figs.append(f'<figure class="shot"><img src="{f}" alt="{t}, {esc(title)}: front, right, back, left, top, bottom" loading="lazy" width="1536" height="1024"><figcaption>{t}: front · right · back / left · top · bottom</figcaption></figure>')
        rows.append(f'<h3>{esc(title)}</h3>' + "".join(figs))
    hands = "".join(f'<figure class="shot"><img src="legib_{t}/{t}_hands_{v}.png" alt="{t} hands close-up frames 1-6, {v}" loading="lazy"><figcaption>{t}: hands, frames 1–6, {v.replace("-", "/")}</figcaption></figure>'
                    for v in ("front", "3-4front") for t in ("v8", "v9") if os.path.exists(os.path.join(SHEETS, f"legib_{t}/{t}_hands_{v}.png")))
    vids = "".join(f'<figure class="shot"><video src="{v}" controls loop muted playsinline preload="metadata" width="512" height="512"></video><figcaption>{c}</figcaption></figure>'
                   for v, c in (("v8_slow.mp4", "v8, slowed"), ("v9_slow.mp4", "v9, slowed"), ("v8.mp4", "v8, 60 fps"), ("v9.mp4", "v9, 60 fps"))
                   if os.path.exists(os.path.join(SHEETS, v)))
    return f"""<section id="v9"><h2>v9 against v8</h2>
<p class="lede">v9 = v8 with the fists straightened: in every frame each fist is turned about the wrist (measured on the Unreal bones) so it sits in line with its forearm with a 10° curl toward the palm and no sideways bend, smoothed over time outside the snap. Fixes the owner's note (rear fist bent inward, front fist fixed forward at frame 3). Everything else is v8: timing, FAF anim 18, contact, guard, legibility. All checks pass. Every key pose below from front, right, back, left, top and bottom (standing rule).</p>
<div class="vids">{vids}</div>
<h3>Hands, frames 1–6</h3>{hands}
{"".join(rows)}
</section>"""

def before_after():
    pairs = [("Contact, game camera behind", "legib_v7/v7_f01_contact_g0.png", "legib_v8/v8_f01_contact_f180.png"),
             ("Contact, 3/4 front (the video's view)", "legib_v7/v7_f01_contact_az315.png", "legib_v8/v8_f01_contact_az315.png"),
             ("Contact, rear", "legib_v7/v7_f01_contact_az180.png", "legib_v8/v8_f01_contact_az180.png"),
             ("Spring, 3/4 rear (v8's remaining flag)", "legib_v7/v7_f08_spring_az225.png", "legib_v8/v8_f08_spring_az225.png"),
             ("Guard at the FAF, 3/4 front", "legib_v7/v7_f17_FAF_az315.png", "legib_v8/v8_f18_FAF_az315.png"),
             ("Retract, side (rear-arm side)", "legib_v7/v7_f04_retract_az270.png", "legib_v8/v8_f04_retract_az270.png")]
    rows = []
    for title, a, b in pairs:
        if os.path.exists(os.path.join(SHEETS, a)) and os.path.exists(os.path.join(SHEETS, b)):
            rows.append(f'<h3>{esc(title)}</h3><div class="pair"><figure class="shot"><img src="{a}" alt="v7, {esc(title)}" loading="lazy" width="1536" height="876"><figcaption>v7</figcaption></figure>'
                        f'<figure class="shot"><img src="{b}" alt="v8, {esc(title)}" loading="lazy" width="1536" height="876"><figcaption>v8</figcaption></figure></div>')
    vids = "".join(f'<figure class="shot"><video src="{v}" controls loop muted playsinline preload="metadata" width="512" height="512"></video><figcaption>{c}</figcaption></figure>'
                   for v, c in (("v7_slow.mp4", "v7, slowed (10 fps)"), ("v8_slow.mp4", "v8, slowed (10 fps)"), ("v7.mp4", "v7, 60 fps"), ("v8.mp4", "v8, 60 fps"))
                   if os.path.exists(os.path.join(SHEETS, v)))
    return f"""<section id="v8"><h2>v8 against v7</h2>
<p class="lede">v8 keeps Mario's windows, the 1-frame startup and the spring. What changed: closed fists on both hands; rear glove by the chin and elbow down on the ribs, moving with the torso and following it 1–1.5 frames late in the recovery; contact with less hip slide (65%) so the chest leads and leans 12.6° in; punch line 12 cm outward and head 6 cm off the line, so the fist shows beside the head from behind; stance 6 cm wider and 3 cm lower; no backward sway after the FAF (FAF now anim 18 = game frame 19). Clearance ≥ 2.7 cm, feet ≤ 0.2 cm.</p>
<div class="vids">{vids}</div>
{"".join(rows)}
</section>"""

def motion_rows():
    import json
    rows = []
    for t in ("v5", "v6", "v7", "v8", "v9", "v11"):
        mp = os.path.join(SHEETS, f"{t}_motion.json")
        if not os.path.exists(mp):
            continue
        m = json.load(open(mp))
        w = m["weight"]; ov = m["overlap"]; hr = m["hold_recoil"]; sp = m["impact_spacing"]
        jit = [f for f in m["flags"] if f["check"].startswith("jitter")]
        def spread(ch):
            r = ov.get(ch, {}).get("recovery", {})
            return r.get("spread_frames")
        rows.append((t, f"{w['spine_tilt_deg']['delta']}°", f"+{w['com_shift_cm']['contact']} cm", f"{w['pelvis_drop_cm']['contact']} cm",
                     f"{w['anticipation_cm']['frames']} frame(s), none",
                     f"lead {spread('lead arm')} / rear {spread('rear arm')}", f"{spread('pelvis > chest')}",
                     "; ".join(f"{f['bone']} {f['windows'][0][0]}–{f['windows'][-1][1]} ({', '.join(f['kinds'])})" for f in jit) or "none",
                     f"{sp['frames']} frame(s): {round(100 * sp['share_last2'])}% in the last 2",
                     f"{hr['hold_frames']} frame(s) (min {hr['hold_frames_min']}), recoil {hr['recoil_effector']['reversals']} rev"))
    return rows

def motion_html():
    head = ("Version", "Spine lean Δ (info)", "Weight shift", "Pelvis", "Anticipation", "Arm overlap spread, frames (≥ 1)",
            "Pelvis→chest lag (≥ 0.5)", "Jitter on arcs", "Impact spacing (≥ 60%)", "Hold / recoil")
    body = "".join("<tr>" + "".join(f"<td>{esc(str(x))}</td>" for x in r) + "</tr>" for r in motion_rows())
    figs = []
    for f, alt, cap in (("impact_spacing.png", "Impact spacing chart and strong-strike profile", "Impact spacing: cumulative % of the fist path per frame (left); default profile for strong strikes (right)"),
                        ("v7_motion.png", "v7 weight, overlap and bone speed charts", "v7: weight and force curves, speed peaks per chain, bone speeds with flagged frames")):
        if os.path.exists(os.path.join(SHEETS, f)):
            figs.append(f'<figure class="shot"><img src="{f}" alt="{esc(alt)}" loading="lazy"><figcaption>{esc(cap)}</figcaption></figure>')
    trajs = []
    for f, cap in (("legib_v7/v7_traj_f08_p090.png", "v7, PvP side (lead side)"), ("legib_v7/v7_traj_f08_az270.png", "v7, side (rear-arm side)"),
                   ("legib_v7/v7_traj_f08_az315.png", "v7, 3/4 front"), ("legib_v7/v7_traj_f08_f180.png", "v7, Follow camera behind"),
                   ("legib_v6/v6_traj_f11_az270.png", "v6, side: the flagged rear-elbow loop (frames 8–16)"),
                   ("legib_v5/v5_traj_f08_az270.png", "v5, side: the flagged lead-hand arc (frames 6–12)"),
                   ("legib_v11/v11_traj_f08_p090.png", "v11, PvP side (lead side)"), ("legib_v11/v11_traj_f08_az270.png", "v11, side (rear-arm side)"),
                   ("legib_v11/v11_traj_f08_az315.png", "v11, 3/4 front"), ("legib_v11/v11_traj_f08_f180.png", "v11, Follow camera behind"),
                   ("legib_v9/v9_traj_f08_p090.png", "v9, PvP side (lead side)"), ("legib_v9/v9_traj_f08_az270.png", "v9, side (rear-arm side)"),
                   ("legib_v9/v9_traj_f08_az315.png", "v9, 3/4 front"), ("legib_v9/v9_traj_f08_f180.png", "v9, Follow camera behind"),
                   ("legib_v8/v8_traj_f08_p090.png", "v8, PvP side (lead side)"), ("legib_v8/v8_traj_f08_az270.png", "v8, side (rear-arm side): no flagged arcs"),
                   ("legib_v8/v8_traj_f08_az315.png", "v8, 3/4 front"), ("legib_v8/v8_traj_f08_f180.png", "v8, Follow camera behind")):
        if os.path.exists(os.path.join(SHEETS, f)):
            trajs.append(f'<figure class="shot"><img src="{f}" alt="{esc(cap)} motion paths" loading="lazy" width="768" height="832"><figcaption>{esc(cap)}</figcaption></figure>')
    return f"""<section id="motion"><h2>Motion: weight, overlap, jitter, impact, hold</h2>
<p class="lede">From the owner's own criteria: weight and force (Terry's forward tilt, after Sakurai), secondary motion, smooth paths without zig-zags on arcs, the strike's impact spacing (long build, 1–2 frame snap) and holding the pose with recoil (Ryu's forward smash). Calibrated on v5–v7, the owner's notes on them, and synthetic self-tests.</p>
<div class="tablewrap"><table><thead><tr>{"".join(f"<th>{esc(h)}</th>" for h in head)}</tr></thead><tbody>{body}</tbody></table></div>
<div class="notes">
<div class="note"><h3>Weight</h3><p>"Forward tilt" was the name of Terry's move (the owner confirmed): Sakurai's example of impact spacing, not a spine lean. The lean is now info only. v5–v7 slide the body 21 cm upright; v8 slides 18 cm and leans 12.6° into the punch, the chest leading the hips.</p></div>
<div class="note"><h3>Overlap</h3><p>v5 moves the torso as one block, the "doll pushed by an outside force" the owner described. v6's arm-led recovery passes. v7 brought back a rigid rear arm (0.17 frames). v8 cascades: hips → chest 1.1 frames, rear shoulder → hand 1.6, lead arm 1.2.</p></div>
<div class="note"><h3>Jitter</h3><p>Measured on arcs, where it shows: circle fit per 7-frame window, radial and out-of-plane wobble, uneven angular speed. Snaps, the spring and hold windows are beats. v6's flagged rear-elbow loop is the arm the owner complained about; v7 keeps a milder rear-elbow wobble.</p></div>
</div>
{"".join(figs)}
<h3>Motion paths</h3><p class="lede">Per-frame dots and lines over the real mesh. Red rings: flagged frames; orange: beats (snap, spring) that are not flagged; dashed white: the fitted arc where a window was flagged.</p>
{"".join(trajs)}
</section>"""

def build():
    rows = "".join(f'<tr><th scope="row">{esc(a)}</th><td class="mono">{esc(b)}</td>{cell(c)}{cell(d)}{cell(e)}{cell(g)}{cell(h)}{cell(v)}<td class="why">{esc(w)}</td></tr>'
                   for a, b, c, d, e, g, h, v, w in ARM_RULES + [r[:7] + (V11[r[0]],) + r[7:] for r in RULES])
    sections = []
    for key, anchor, name, fr in POSES:
        figs = []
        for v, label in VIEWS:
            f = f"legib_v7/v7_{key}_{v}.png"
            if os.path.exists(os.path.join(SHEETS, f)):
                figs.append(f'<figure class="shot"><img src="{f}" alt="v7 {esc(name)}, {esc(label)}: mesh render beside the per-part colour render" '
                            f'loading="lazy" width="1536" height="876"><figcaption>{esc(label)}</figcaption></figure>')
        sections.append(f'<section class="pose" id="{anchor}"><h3>{esc(name)}<span class="fr">{esc(fr)}</span></h3>{"".join(figs)}</section>')
    heat = "".join(f'<figure><img src="{t}_legibility.png" alt="{t} legibility heatmap" width="1200" height="1920" loading="lazy">'
                   f'<figcaption class="cap">{t}</figcaption></figure>' for t in ("v11", "v9", "v8", "v7", "v6", "v5"))
    return f"""<title>Jab Legibility Review</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@500;700&family=Source+Sans+3:wght@400;600&family=JetBrains+Mono:wght@400;600&display=swap">
<style>{CSS}</style>
<main>
<header>
  <h1>Jab Legibility Review</h1>
  <p class="lede">Each body part of the UEFN mannequin (its physics-asset capsules) is projected on its own, frame by frame, from 8 orthographic views plus the game camera, so overlaps inside the silhouette are measured instead of hidden. The thresholds were calibrated on v5, v6 and v7 against the owner's notes on v7: rear hand open, rear elbow drifting toward the back, raised rear arm over the fist.</p>
  <p><span class="verdict ok8">v11: every check PASS</span> <span class="verdict">v9: front elbow out on frames 4–10 (new arm check)</span> <span class="verdict">v8: wrists and elbow fail</span> <span class="verdict">v7: 5 rules + motion fail</span></p>
</header>
<nav aria-label="Sections"><a href="#rules">Rules</a><a href="#v11">v11 vs v9</a><a href="#v9">v9 vs v8</a><a href="#v8">v8 vs v7</a><a href="#motion">Motion</a><a href="#contact">Contact</a><a href="#retract">Retract</a><a href="#spring">Spring</a><a href="#exit">Exit</a><a href="#faf">FAF</a><a href="#heatmaps">Heatmaps</a><a href="#spotcheck">Mesh check</a></nav>
<section id="rules"><h2>Rules and flagged frames</h2>
<div class="tablewrap"><table><thead><tr><th>Rule</th><th>Limit</th><th>v5</th><th>v6</th><th>v7</th><th>v8</th><th>v9</th><th>v11</th><th>Why this limit</th></tr></thead><tbody>{rows}</tbody></table></div>
</section>
<section class="notes" aria-label="The owner's three points">
  <div class="note"><h3>Rear hand open</h3><p>Confirmed. The right hand's fingers are never animated (93.7°, the reference pose). The left hand is a fist only during the punch (209° on frames 1–8), then opens again: 94° at guard and FAF.</p></div>
  <div class="note"><h3>Rear elbow toward the back</h3><p>At contact v7 keeps the elbow 4.4 cm behind the back plane (v5/v6: 11.8 cm, so better but still behind). In the recovery v7 keeps it outside the body, 12–21 cm outside the outline from the video's 3/4 view, while v5/v6 tucked it in on frames 7–10. The guard itself has the elbow 41 cm to the side of the chest centre; that comes from the blockout.</p></div>
  <div class="note"><h3>Game camera</h3><p>The project's gameplay camera is GSTNCamera's Follow mode: pivot at the actor location (88 cm above the feet), 650 cm arm, 25° above, FOV 65, free orbit. From straight behind, the jab at contact goes along the camera axis, behind the head: only 10% of the arm's side-view area reads. On a 16:9 screen the character is about 39% of the height (the Kill la Kill reference is about 1/8). Silhouette width/height at contact 0.37–0.54. Fix by pose: punch line slightly outward, head and shoulders slipping off the line, more torso turn.</p></div>
  <div class="note"><h3>Raised rear arm over the fist</h3><p>From the video's 3/4 front view the rear arm never covers the fist. It does from behind at contact (rear hand on the face) and from the 3/4 rear on the way back to guard (frames 13–17, up to 46%). In the guard itself the rear arm covers 47% of the fist from that view.</p></div>
</section>
<section><h2>Key poses, five views</h2><p class="lede">The game-camera shots are a centred crop of the real 65° view from the real camera position, so overlaps are exact. Left: the real mesh rendered in Unreal. Right: the same camera with each part in its own flat colour. Red contour: the attacking fist, drawn even where it is hidden. Black line: the mesh outline. The numbers in each header are measured on that image.</p></section>
{"".join(sections)}
{v11_vs_v9()}
{v9_vs_v8()}
{before_after()}
{motion_html()}
<section id="heatmaps"><h2>Heatmaps, frames by views</h2><div class="heat">{heat}</div></section>
<section id="spotcheck"><h2>Capsules against the mesh</h2>
<div class="spot"><img src="v7_legibility_spot_f14_az225.png" alt="Grey: Unreal mesh silhouette. Black: capsule outline. Red: fist capsule. Blue: rear arm capsules. Purple: their overlap." width="512" height="512">
<div><p>Worst v7 case (frame 14, 3/4 rear): orthographic SceneCapture from Unreal against the capsule masks gives an overlap (IoU) of <b>0.86</b>, and 85% of the fist mask lies on mesh pixels. In the 21 large shots the overlap is 0.70–0.84: the capsules are slightly slimmer than the body.</p>
<p class="cap">grey = mesh · black = capsule outline · red = fist · blue = rear arm · purple = overlap</p></div></div>
</section>
</main>
"""

if __name__ == "__main__":
    out = os.path.join(SHEETS, "legibility_review.html")
    open(out, "w", encoding="utf-8").write(build())
    print(out)
