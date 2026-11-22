# runstart-w3 + gstn-anim-cycle-cheap (desktop, 2026-11-22): w3 5/6 SI + skill cheap
Branch `claude/runstart-generic-desktop` (`00c81ad3` per `c2/fine/`). `w1` (20) e `w2` (21) intatti; `DA_Locomotion_Default.uasset` / `CHT_*` / `Mixamo/` intoccati.

## Fatto
- Creata variante additiva `w3` (variante `29`, `Tools/AnimPipeline/cards/runstart/runstart_w3_ue.py`): caduta in avanti dalle caviglie ancorata ai contatti di `w2`, onda `pelvis->spine->head`, `arm_r`/`arm_l` staccati e ammorbiditi (`elb_r` `169.2°->133.9°` a `v92`), settle `v107->v137`, spinta `leg_l` a `v107` (`134.7°`), punte `ball_l/r` rilassate in volo (`roll_l 13.0°`, `roll_r 13.7°`), mento `+6.5°`.
- Check su `c2/fine/`: `GATES: OK` (`bone_worst 0.5%`, `fails: []`), `ASSERT: OK` (`min 1.00`), Critico Estetico `0 grave` nella clip (`v92-v197`), Reviewer `5/6 SI` (`peso`, `tempo`, `root`, `aggancio`, `pulizia`).
- Video A/B `c2/fine/`: `docs/notes/reviews/runstart-w3-c2/fine/ab_21_w2_vs_29_w3_{az90,game}_{real,slow4x}.mp4`.
- Skill: creata `gstn-anim-cycle-cheap` (esecutore + gate/cattura su DeepSeek/deterministico, modello capace solo su `fine/`), junction in `~/.gemini/config/skills/` per tutte le 5+1 skill, aggiunta porzione deterministica a `gstn-anim-executor/project.md`.

## Mini aggiustamenti rimasti su w3 (da rivedere dopo)
1. **`arm_r` fase `v92 (f5)` (`vita: NO`):** ruotare `upperarm_r` avanti in opposizione a `leg_r` dietro (elimina doppio cambio `v92 dietro -> v107 avanti -> v122 dietro`).
2. **Gomiti `v107` (lieve/medio):** ~10° più larghi in abduzione per massima leggibilità da dietro (`game`).
3. **Mento `v107` (lieve):** +5° circa verso l'orizzonte.
4. **Cattura batch `v77` (`gioco`/cattura):** fix reset yaw iniziale in `PIE_CAPTURE_RUNS` (in `c2/fine/` `v77` parte ruotato ~60° prima dell'inizio clip).
5. **Gate deterministico:** aggiungere check opposizione di fase `upperarm` vs `thigh` in `runstart_pivot_gates.py`.

## Prossimo
- Turnaround `180L` con `gstn-anim-cycle-cheap` (esecutore su worktree sparse DeepSeek `Tools/CI/New-DeepSeekWorktree.ps1`).

COSTO: ~0 $ | ~45 min desktop | quota_pro=~50% settimanale (agy_student)
