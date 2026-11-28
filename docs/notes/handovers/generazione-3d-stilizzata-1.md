# Stylized 3D generation research + head base tests (2026-11-27/28)

Thread: research on generating 3D models from a heavily stylized art style (owner request), then hands-on attempts at a
head base for the character creator. Research only: no game code, nothing in Content/.

## Delivered
- Report with verdicts and sources ([V] verified / [T] third party / [N] ours): project files
  `strumenti/generazione-3d-stilizzata-2026-11-27.md` (sections 1-8; row added in `anim-theory/indice-ricerca.md`).
  Core verdict: no tool holds a heavy stylization on its own; style is locked in 2D, AI gives a volume draft at most;
  skin, UVs, topology and creator seam rings stay human/template work. Hunyuan3D licence excludes the EU (study only).
- Owner art direction recorded in memory `artstyle-direzione`: Date A Live manga (Ringo) mix of cartoon and anime,
  Western+Asian features, PBR like DK Bananza, NOT cel shaded; target 2D->3D fidelity like the 3D film "Lupin III: The First".

## Attempts and outcomes (all judged by the owner)
1. Procedural ortho proxy (`character-creator/ortho-kit-v1/` in project files, `head_proxy.py` + `params.json`):
   measured front table + profile traced from manga panels. Owner: "non funziona". Do not push it again.
2. Blender MCP live test on the LAPTOP (Blender 5.3, addon socket 9876; the RC session spoke to the socket directly).
   Files: laptop `GASP_Giustino2\Saved\BlenderRefs\11_Blender_Head\` (full copy of F:\Date A Live sits in `Saved\BlenderRefs\`).
   - v1, v2: quad cube + table-driven shape. v1 had mis-registered profile refs (nose tip read at eye level). Owner on v2:
     face flattened, nose must be clearly pointed, cheek-to-chin inversion SUBTLE, follow the Ref_Front contour.
   - v3: owner's VRoid export (`base\to.vrm`, imported as .glb) + shape keys `Fit_Refs` and `Western`.
     Owner stopped here (option C): MCP moves vertices by code, it does not sculpt.
3. Tripo question: owner agreed it will not handle the drawn-angle "cheats" well; a free-tier TEST on Tohka (not our
   design, so public output is acceptable) is the next step. Input set ready on the laptop in
   `Saved\BlenderRefs\12_Tripo_Input\` (crops 1024x1024, eyes y420, chin y620, LEGGIMI.md with page/panel per crop).
   Try 1 multi-view: Front 01_fronte_c5p11, Back 04_retro_c6p03, 3/4 02_34_guarda-dx_c6p19, Side EMPTY.
   Try 2 single image: 00_RefFront_normalizzata. Coherence medium-low (nose 21-30%, mouth 70-85% of eye-chin);
   Ref_Front is wider/shorter than the manga (eyes ~30% further apart), so do not mix it with the panels.
   **Caveat: Ch.1-6 has NO Tohka profile. The profile refs used for ortho-kit v1 and Blender v1-v3
   (`Vol.1 Ch.1 p10`, `Ch.4 p24`) appear to be Shido (male lead), not Tohka.**

## Lessons
- Shape from measured tables gives "non-human" errors; the owner's eye judges shape. Show overlays before calling
  anything a base. Register a reference with two landmarks, never by eye.
- Drawn faces are not one consistent volume: 1:1 fidelity across angles needs a sculptor checking against model
  sheets and, later, view-dependent corrective shapes in the face rig.

## Open for the coordinator / owner
- Owner runs the Tripo free test with the 12_Tripo_Input set and sends the GLB; then overlay it with the same ortho
  cameras against the 12_Tripo_Input crops and the ch1 p40 3/4 panel (no Tohka profile exists in Ch.1-6).
- Research pilot (Tripo / Rodin / Meshy paid, ~55-80 $ for a month) still unanswered.
- Who sculpts the real head (owner or an artist); Claude supports with overlays, checks and the UE pipeline.

ESITO: PARZIALE research report delivered; head base not reached (proxy and Blender MCP v1-v3 rejected or stopped by owner)
SUBAGENTI: 3 Remote Control sessions (desktop/laptop, Blender MCP modeling and ref analysis), Opus
COSTO: ~35 $ (estimate, not metered) | 0 min desktop (laptop ~2 h RC) | 0 token review
