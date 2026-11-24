# turnaround-180l-c1 (2026-11-23): Turnaround 180L (variante 28, AS_GSTN_RunStart_Generic_w2t180Ls)
Branch: `claude/deepseek-turn4` (worktree `deepseek_turn4`). Pipeline `gstn-anim-cycle-cheap`.

## Interventi applicati (`Tools/AnimPipeline/cards/runstart/turnaround_180l_ue.py`, idempotente via `w2t180Ls_base_tracks.json`)
1. **Unico arco continuo f0-f27**: smoothing temporale continuo (3 pass) su `pelvis`, `spine_01..03`, `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r` per eliminare il doppio battito al hand-off `f6-f8` con fade-out entro `f27`.
2. **Spalle alte/aperte**: `T180L_CLAV_ELEV=7.0°`, `T180L_CLAV_OPEN=5.0°`, `T180L_ARM_OUT=10.0°` (`f2-f20`).
3. **Braccio sinistro oltre il piano del busto dietro**: `T180L_ARM_L_BACK=26.0°`, `T180L_ARM_L_ELBOW=14.0°` (`f2-f13`, picco `f4-f9`), gomito naturale e clearance sicura.
4. **Banking del torso già nel pivot**: `T180L_TORSO_BANK=11.0°` (verso interno L) + `T180L_TORSO_LEAN=5.0°` su `spine_01..03` (`f2-f22`).

## Gate, Cattura e Review finale
- `runstart_pivot_gates.py --json ... 12`: **`GATES: OK`** (`gap_min_win=12.1 cm` f11, `bone_worst=0.5%`, `jit_knee_l=9.8`, `clear_hand_l-thigh_l=18.6 cm`, `clear_hand_l-hips=25.7 cm`, `roll_l=17.4°`, `roll_r=11.2°`).
- Cattura GPU Play A/B (`27_p` vs `28_s`, `mv_turn_180a` window `88..132`): **`ASSERT: OK`** su entrambe (`27_p` e `28_s`).
- Review finale su `docs/notes/reviews/turnaround-180l-c1/fine/grid_ab_az90.jpg` e `grid_ab_game.jpg`: **PASS** — rispetto a `27`, `28` mostra chiaramente a `v92` banking del torso già nel pivot, spalle alte/aperte, braccio sinistro esteso dietro oltre il piano del busto e arco continuo fluido `v92->v197` fino al loop.
- Nota tecnica: `claude.exe` su DeepSeek è andato in timeout sui tool call complessi in `deepseek_turn4` (smoke test `deepseek-flash` e `deepseek-v4-pro` senza tool OK in 5-7s, ~$0.21); authoring eseguito dal coordinatore + esecuzione UE/cattura deterministica a 0 token.

COSTO: 0.21 $ (smoke test DeepSeek) + 0 $ agy_student | provider=deepseek+agy_student | quota_pro=0%
