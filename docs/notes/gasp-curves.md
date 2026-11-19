# GASP curves and notifies: what they do, what our clips carry (2026-11-19)

Sources: Saved/Routing/curves_audit.txt (Tools/GAS/routing/curves_audit.py: every GASP clip vs ours), the names stored in
ABP_GSTN_Locomotion.uasset, and the Unreal Editor AI assistant (asked through AIAssistantInstance.send_text_message_and_get_reply).

## Rule that matters most
A curve a clip does not have reads 0. So an absent `Enable_*` curve turns the feature OFF, an absent `Disable_*` leaves it ON.
While the Blend Stack blends two clips, the one without the curve weighs as 0.

## What each one does (in our ABP; "ABP" = the name is in ABP_GSTN_Locomotion)
| Curve / notify | Read by | Effect |
|---|---|---|
| MoveData_Speed (ABP) | dynamic play rate | the clip's own speed; play rate = capsule speed / this |
| MaxDynamicPlayRate (ABP) | same | cap of that play rate |
| Enable_Warping (ABP) | stride + orientation warping alpha | 1 = stride and facing adapt to the capsule |
| Enable_TurnInPlaceSteering (ABP) | steering, turn in place | root steering during a turn in place |
| Disable_AdditiveLeans, Disable_AO (ABP) | lean additive, aim offset | 1 = no lean / no aim offset on that clip |
| contact_l/r (ABP: contact history) | foot contact history | which foot is planted |
| phase | pose search / foot cycle | position in the step cycle |
| steeringtargettime, disable*footik, disablepelvisadjustment, distance_from_ledge | steering, foot IK, traversal | not used by our states |
| PoseSearch* notify states | Pose Search databases only | none for our Blend Stack clips (not in a database) |
| Foley notifies (Run/Walk/Land/Scuff) | footstep sounds | without them our clips are silent |

## GASP parity applied to our clips (Tools/GAS/routing/curves_author.py, derived from each clip's own bones)
| Clip | Kind (GASP equivalent) | Added |
|---|---|---|
| AS_RunStop_Ganon_L_V5 / R_V5 | stop (M_Neutral_Run_Stop_*) | contact_l/r, movedata_speed, Foley Run L/R |
| AS_GSTN_RunStart_Frantic | start, but in place (no root motion) | contact_l/r, phase, Foley; NO enable_warping / movedata_speed (warping would scale by a 0 root speed) |
| SS_180_Turn_Root180 (skid, owner's clip) | pivot (M_Neutral_Run_Pivot_*) | contact_l/r, movedata_speed, phase, Foley; NO enable_warping (the slide is wanted) |
| AS_GSTN_Crouch_* | - | skipped: the clips have no bone tracks (known, crouch_fk_build) |
Contact rule checked on GASP clips: 93-99% agreement with GASP's own contact curves; movedata_speed matches (500 cm/s run start).
Not done: Disable_AdditiveLeans / Disable_AO = 1 on skid and stop (GASP's stops do not have them; try it if the lean bends the skid).
