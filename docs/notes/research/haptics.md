# Haptics research: moves and footsteps (2026-11-21)

Scope: cloud research, no code. Target: UE 5.9, Xbox pad on PC now, DualSense and Switch 2 later. Confidence note: built from Epic docs/forum titles and general knowledge; API names marked (verify) must be checked in the 5.9 editor before coding (ask the Unreal Assistant first, per project rule).

## 1. What UE offers natively
- **Force Feedback Effect asset** (`UForceFeedbackEffect`): curves per channel (large/small motor, plus left/right trigger on pads that have them). Played by `Client Play Force Feedback`, `Spawn Force Feedback at Location` (with attenuation), and the **AnimNotify "Play Force Feedback"** (usable in montages and sequences, so footsteps/hits can sit on the clip itself). Works on Xbox pad via the stock XInput/GameInput path.
- **Haptic Feedback Effect** (Curve / Buffer / SoundWave): the haptic path for mobile/VR/DualSense-style devices; Buffer and SoundWave variants carry audio-rate data. On desktop pads support is device dependent (verify).
- **Input Device Subsystem + Device Properties** (5.2+/5.6): `UInputDeviceProperty` assets (light color, **trigger resistance/vibration/effect**) applied per input device by `Set Device Properties`. The generic engine API exists; the actual DualSense backend is what varies (see 2).
- **Enhanced Input**: Input Mapping Contexts can attach feedback; useful for trigger effects on action (e.g. RT strong attack). Not needed for hits.
- Xbox impulse triggers: exposed by GameInput on Windows (verify in 5.9) via trigger motors; stock Force Feedback does not always drive them.

## 2. DualSense and Switch 2
- Sony's native DualSense plugin is console-side. On PC, community/marketplace plugins fill the gap: **Windows DualSense** (Rafael Valoto, Fab, UE 5.2~5.8, includes audio-over-Bluetooth haptics and adaptive triggers), **SenseCore** (UE5 forum), **Adaptive Pad** (Altercode: triggers, gyro, haptics, lights). Open-source base used by several: ds5w / DualSense-Windows (verify license before shipping).
- **Steam Input**: abstracts Xbox/DualSense/Switch pads and exposes haptic and trigger-rumble calls through Steamworks; good for a Steam release, adds a dependency.
- **Switch 2**: Joy-Con 2 HD rumble (quieter than Switch 1) is only reachable through the Nintendo SDK, under NDA. Design so nothing is Xbox-specific: keep effects as abstract data, map per device at the end.
- Perf: rumble is cheap CPU-wise; audio-haptics adds an audio stream, so keep it opt-in and off for the Switch 2 profile at first.

## 3. How Astro Bot / Smash do it (reference)
- Astro Bot (Team Asobi): haptics are largely **audio-derived**: each surface (metal, sand, glass, water) gets its own footstep haptic texture, played from the step event, scaled by speed and weight; landings, hits and collectibles have short distinctive bursts; adaptive triggers for gadgets. Principle: haptic = a tiny designed sound of the event, not a constant buzz.
- Smash (Nintendo): short, sharp **HD rumble pulses on hit**, stronger on strong hits and KOs, tied to hitstop; weak moves and footsteps nearly silent. Principle: rumble length follows hitstop, intensity follows damage/knockback.
- Takeaway for us: footsteps = very light pulse per plant, surface and speed dependent; hits = pulse length = hitstop, strength = move strength; landings = one thump scaled by fall speed.

## 4. Recommendation
**A. Data-driven haptic table on the presentation layer, backed by stock Force Feedback Effects (works on Xbox now).**
- One `DataAsset` (event tag -> Force Feedback Effect, intensity scale, min magnitude, optional per-surface override), same shape as the existing presentation table (event -> Niagara/sound/shake, parent-tag fallback, kit/skin stack). Events: footstep plant (the same notify that fires stock GASP foley), hit given/taken (magnitude from hitstun), hitstop, landing, bounce, KO.
- Footsteps: reuse the foley notify; add the haptic as a second action on the same event, scaled by speed gait and surface.
- Fire on the 62.5 Hz sim event (not render) so it syncs with hitstop; dedupe like other presentation events.
- Everything tunable in DA; BP event `OnHapticRequested`; **debug menu (GSTN Debug Options): one checkbox "Haptics" plus one dropdown "Haptic intensity (Off/Low/Normal/High)"**, as required by project rule. Also an accessibility-style master scale in settings.
- Device layer behind a tiny interface (`IGSTNHapticBackend`): default backend = Force Feedback; later backends plug in without touching tables.

**Alternatives**
- **B. AnimNotify-only** (Play Force Feedback notify on clips): fastest to start, zero code, but intensity not data-driven, not tied to sim events/hitstop, hard to scale per surface. Fine as a quick prototype for footsteps.
- **C. Add DualSense plugin (Windows DualSense / Adaptive Pad) as a second backend + Steam Input later**: richest feel (audio haptics per surface, triggers on RT/LT) but third-party dependency, license and UE 5.9 compatibility to verify; do it after A works, as an optional backend.

## 5. Next steps (if approved)
1. Thread (Sonnet, small, ~5 $): table DA + Force Feedback backend + footstep and hit hooks + debug-menu entries + 5 sample FFE assets.
2. Ask the Unreal Assistant which Device Property / GameInput features 5.9 exposes for impulse triggers.
3. Decide on DualSense plugin only if the owner has a DualSense to test with.
