# Handover: haptics + surface audio research (2026-11-21)

Branch claude/project-thread-w299nb. Cloud research only, no code, no PR.

- Files: docs/notes/research/haptics.md, docs/notes/research/surface-audio.md.
- Recommendation: one data table keyed by (event, footwear, floor) driving both sound (MetaSound, stock GASP foley as fallback) and rumble (stock Force Feedback Effects, backend interface for DualSense/Steam/Switch later).
- Owner decisions: start from stock GASP sounds; footwear types per SKIN = Metal (knight boots), Clay-like (stock default), Barefoot, Shoe sole; floors = GASP Physical Materials.
- Rules: every setting also in GSTN Debug Options (one control each); GASP divergence row in gasp-divergenze.md before commit; ask the Unreal Assistant how stock GASP picks foley first.
- Not verified: exact GASP foley class names and 5.9 API names (marked verify; sources were forum/doc titles only).
- Next (separate thread, Sonnet medium, desktop editor needed for asset checks, ~6-8 $): footwear enum on skin data + GSTN_SurfaceResponseTable + footstep/landing wiring + Force Feedback backend + debug entries.

COSTO: ~3 $ | 0 min desktop | 0 token review
