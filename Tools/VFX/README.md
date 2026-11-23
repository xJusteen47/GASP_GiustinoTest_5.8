# Tools/VFX

`vfx_spec_check.py`: blocking gate before a VFX capture goes to review (audit 2026-11-20, §2.4). It reads one spec per effect in
`specs/` (layers + the material presets they use) and checks the owner's VFX rules offline in seconds:

| Rule | Kinds | FAIL when |
|---|---|---|
| layers | effect | fewer than `min_layers` (2) layers |
| grow_only | dust | the drawn size (flipbook silhouette x quad scale) drops > 0.5 % between two life steps (owner 2026-11-19: dust never shrinks) |
| lit | dust, solid | the material is self-lit/unlit (presets in `LOOKS`); scene-lit = `LIT_LOOKS` or `"material": {"shading": "lit"}` |
| end_alpha | dust, solid, flash | alpha at the end of life > 0.01 |

Run: `python Tools/VFX/vfx_spec_check.py --all` (or one spec). Needs numpy + Pillow. Prints `VFX_SPEC: PASS/FAIL` + max 3 lines,
report in `Saved/VfxSpec/vfx_spec_check.txt`. Exit 0 / 1 / 2.
New effect: add `specs/<effect>.json` (format in the script's docstring); a layer is a `preset` of
`Tools/GAS/setup_presentation_vfx_puff.py` or explicit `size`/`alpha` curves over life. Owner rules keep their scope through
`kind` (dust / solid / flash): solids may shrink, flashes may be self-lit.
It replaces `Tools/GASNPPTest/vfx_size_monotonic.py` (same size model; that script lives on the VFX branch, delete it there when
the branches meet). Tests: `python Tools/VFX/tests/test_vfx_spec_check.py`.
