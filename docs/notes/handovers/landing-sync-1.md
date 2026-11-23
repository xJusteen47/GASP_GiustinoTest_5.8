# Landing sync 1 (desktop _ue59, Opus, 2026-11-19 15:35-16:55 UTC) - landing PASS, satin + hub look done, run stop/skid/dash stopped
Branch claude/locomotion-finish-3 (from 9f3156ae). Reviews docs/notes/reviews/{landing-sync,runstop-v5,runstop-v6,skid-a5,skid-a5b}.
1. Landing dust vs pose: PASS (landing-sync/review_fable.md). Cause: 32 GASP landing rows of CHT_GSTN_LocomotionAnimations blended
   0.2 s; now 0.05 s (Tools/GAS/routing/land_blend.py, Saved/Routing/land_blend.txt "changed 32"). Dust f81 = crouch 96% (base 24%).
   Open: left arm fling of the GASP clip now visible; 1-2 repeated frames at every state entry (gioco, also run stop/skid).
2. Satin look (owner): MI_GSTN_Mannequin_Satin (Tools/GAS/setup_satin_look.py, metallic 0.35/0.55, roughness 0.30/0.34, colours kept)
   + cvar/checkbox gstn.Debug.SatinMetal (UGSTN_SatinLookSubsystem). Image landing-sync/satin_vs_original.png.
3. GASP curves (owner): docs/notes/gasp-curves.md (editor AI assistant answer + ABP name table). curves_author.py writes contact_l/r,
   phase, enable_warping (root-motion starts only), Foley Run L/R. NEVER movedata_speed: Get_DynamicPlayRate then plays our clips at
   capsule speed / clip speed (skid stalled at speed 0). Crouch clips skipped (0 bone tracks).
4. Test hub (owner): GASP LevelVisuals + grid materials (Tools/GAS/hub_gasp_look.py, backup Saved/Backups/L_Test_Hub_before_gasp_look.umap).
5. Run stop V5 FAIL, V6 FAIL (2 attempts used). V6: twist fixed, all keys play, but after the clip the body drifts 3-5 cm for 35 f: the
   game has TWO rest idles ~4 cm apart (runstop-v6/review_fable.md item 1); one arm still drops back alone. Next: log the idle asset/time
   and offset root after a stop (150 f) + one run with Mode 0, find why the rest idle differs. Then pose round: owner decides (rule 10).
6. Skid a5 / a5b FAIL (2 attempts used). New getter GetAnimOwnerStickAlong; Skid Exit Run leaves at once on release (release case
   clean: 0 frozen, plants < 2 cm; loop drag 41 -> 16 cm). Still: right foot pop ~f137 tied to clip time, two different skid poses for
   the same input (body yaw?, not logged), 30 cm slide at the hand-back. Next: latch body yaw to the clip reverse->exit, log actor/mesh
   yaw; capture lane off the kit disc.
7. L3 dash r3 imported (Tools/GAS/dash/dash_states.py OK: root 227 cm, Dash state + alias + row). Capture: the boost never fires in the
   capture (combat tap and IA_GASNPP_RunBoost press both = plain run start, Saved/Dash/cap1, cap2): 2 attempts. Next: find how L3 reaches
   BoostCommand in a scripted PIE (stick held first? trigger type?), then capture mv_dash vs mv_dash_base and Fable review.
8. 3-PIE perf repro: 2 timeouts, editor hung at startup right after Turnkey (Saved/PiePerf/before_one*). Probe ready:
   Tools/GASNPPTest/pie_perf_probe.py. Retry in a new checkpoint; look at the screen if it hangs.
Not started: run start stomp half height, air crouch landing rise. Capture rows log 'abp' but the call fails (no state-machine API
in Python): ABP state still not logged. Lock released. Cost ~1 h 20 desktop, 3 builds, ~20 headless runs, 9 captures, 5 Fable reviews
(~690k subagent tokens). Proof: numbers above cite the review files; reviews cite frames.jsonl of each capture.
