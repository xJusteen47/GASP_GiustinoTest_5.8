# Handover render-forward-1 (cloud, 2026-11-20)

Goal: groundwork for dropping Lumen / forward rendering, reversible, no project defaults changed. Branch
`claude/project-thread-xmve9g` from `claude/ue59-adoption-1` @49e02284.

Done
- `Source/GASP_Giustino2/Private/Rendering/GSTN_RenderProfile.cpp`: cvar `gstn.Render.Profile` 0..3 (runtime, restores
  originals) + command `gstn.Render.Info`. NOT COMPILED (cloud, no UE). Uses only Core console APIs; first desktop build
  must check it (expected risk: none beyond a missing cvar name in 5.9, which only logs a warning).
- `Tools/Rendering/Open-Editor-Forward.ps1` + `Apri editor FORWARD.bat`: forward shading via `-ini:` overrides, nothing
  written to config. NOT RUN. Verify with `gstn.Render.Info` -> `r.ForwardShading = 1`.
- `docs/notes/rendering-profiles.md`: what forward changes, flicker test order, decisions.

Not done / next
- Desktop: build, then in the viewport `gstn.Render.Profile 1`, then `2`, note which stops the flicker (2 min, no restart).
- Forward launch costs a long first shader compile: only on owner go (decision 1 in the doc).
- Default renderer and AA: owner decisions, in the doc.

COSTO: ~3 $ | 0 min desktop | 0 token review
