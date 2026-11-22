"""Build authentic Overlord Jab suite with full procedural authoring:
1. Updates overlord_jab1.json with Ganondorf OoT backhand slap & folded left arm author poses.
2. Updates overlord_jab2.json with forward lunge (+18cm), villain claw slap, moving hold staring at hand, and decaying tremor author poses.
3. Runs animpipe author on both cards -> writes takes to Unreal via ue_write_take.
4. Runs animpipe build on both cards -> bakes Control Rig to AS_OverlordJab1 and AS_OverlordJab2 in the new folder structure.
5. Assembles LS_OverlordJab_Preview and AM_OverlordJab in Sequencer.
"""
import os, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

JAB1_CARD = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab1.json")
JAB2_CARD = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab2.json")

def setup_cards():
    # --- JAB 1 ---
    with open(JAB1_CARD, "r", encoding="utf-8") as f:
        c1 = json.load(f)
    
    c1["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordJab1"
    c1["anim"] = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1"
    c1["author"] = {
        "take": "AS_Pilot_OverlordJab1",
        "frames": 14,
        "thumb": "open",
        "poses": {
            "0": {
                "pelvis": {"loc": [0, 0, 0], "rot": [0, 0, 0]},
                "spine": [-5.0, 0, 0],
                "head": [5.0, 0, 0],
                "arm_l": {"hand": [20.0, 22.0, -10.0], "pole": [45.0, 10.0, -30.0]},
                "arm_r": {"hand": [-20.0, 25.0, -12.0], "pole": [-45.0, 10.0, -30.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0, 0, 0],
                "wrist_out": [0, 0, 0]
            },
            "2": {
                # Anticipation: torso coils internally, left arm folds tightly across torso
                "pelvis": {"loc": [0, -1.5, -1.0], "rot": [-2.0, 0, -18.0]},
                "spine": [-8.0, 0, -22.0],
                "head": [8.0, 0, 32.0],
                "arm_l": {"hand": [-10.0, 12.0, -18.0], "pole": [28.0, 15.0, -20.0]},  # folded across chest
                "arm_r": {"hand": [8.0, 18.0, 0.0], "pole": [-35.0, 25.0, 5.0]},       # chambered inside
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [15.0, 38.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-22.0, 18.0, -40.0]},
                "fingers": [0.4, 0, 0],
                "wrist_out": [15.0, 0, 0]
            },
            "4": {
                # Strike: Ganondorf OoT Backhand slap snap! Left arm pinned to torso
                "pelvis": {"loc": [0, 4.0, -0.5], "rot": [-3.0, 0, 20.0]},
                "spine": [-4.0, 0, 28.0],
                "head": [4.0, 0, -18.0],
                "arm_l": {"hand": [-12.0, 10.0, -18.0], "pole": [25.0, 12.0, -20.0]},  # pinned to torso
                "arm_r": {"hand": [-52.0, 42.0, 2.0], "pole": [-40.0, 15.0, -15.0]},   # outward backhand whip!
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [22.0, 42.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-18.0, 22.0, -40.0]},
                "fingers": [0.8, 0, 0],
                "wrist_out": [30.0, 0, 0]
            },
            "6": {
                # Followthrough apex
                "pelvis": {"loc": [0, 4.5, 0.0], "rot": [-2.0, 0, 24.0]},
                "spine": [-2.0, 0, 30.0],
                "head": [2.0, 0, -20.0],
                "arm_l": {"hand": [-10.0, 10.0, -18.0], "pole": [26.0, 12.0, -20.0]},
                "arm_r": {"hand": [-56.0, 38.0, 0.0], "pole": [-42.0, 12.0, -18.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [22.0, 42.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-18.0, 22.0, -40.0]},
                "fingers": [0.7, 0, 0],
                "wrist_out": [25.0, 0, 0]
            },
            "8": {
                # Recoil / Combo cancel window
                "pelvis": {"loc": [0, 2.0, 0.0], "rot": [-1.0, 0, 12.0]},
                "spine": [-4.0, 0, 14.0],
                "head": [4.0, 0, -10.0],
                "arm_l": {"hand": [-5.0, 14.0, -15.0], "pole": [32.0, 12.0, -25.0]},
                "arm_r": {"hand": [-40.0, 30.0, -5.0], "pole": [-45.0, 12.0, -25.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0.4, 0, 0],
                "wrist_out": [15.0, 0, 0]
            },
            "11": {
                # Settle toward guard
                "pelvis": {"loc": [0, 0.5, 0.0], "rot": [0, 0, 4.0]},
                "spine": [-5.0, 0, 4.0],
                "head": [5.0, 0, -3.0],
                "arm_l": {"hand": [12.0, 18.0, -8.0], "pole": [40.0, 10.0, -28.0]},
                "arm_r": {"hand": [-25.0, 26.0, -7.0], "pole": [-45.0, 10.0, -28.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0.1, 0, 0],
                "wrist_out": [5.0, 0, 0]
            },
            "13": {
                # Neutral guard
                "pelvis": {"loc": [0, 0, 0], "rot": [0, 0, 0]},
                "spine": [-5.0, 0, 0],
                "head": [5.0, 0, 0],
                "arm_l": {"hand": [20.0, 22.0, -10.0], "pole": [45.0, 10.0, -30.0]},
                "arm_r": {"hand": [-20.0, 25.0, -12.0], "pole": [-45.0, 10.0, -30.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0, 0, 0],
                "wrist_out": [0, 0, 0]
            }
        }
    }
    with open(JAB1_CARD, "w", encoding="utf-8") as f:
        json.dump(c1, f, indent=1)
    print("Updated overlord_jab1.json with author block")

    # --- JAB 2 ---
    with open(JAB2_CARD, "r", encoding="utf-8") as f:
        c2 = json.load(f)

    c2["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordJab2"
    c2["anim"] = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab2/AS_OverlordJab2"
    c2["author"] = {
        "take": "AS_Pilot_OverlordJab2",
        "frames": 31,
        "thumb": "open",
        "poses": {
            "0": {
                # Entry from Jab 1
                "pelvis": {"loc": [0, 2.0, 0.0], "rot": [-1.0, 0, 12.0]},
                "spine": [-4.0, 0, 14.0],
                "head": [4.0, 0, -10.0],
                "arm_l": {"hand": [-5.0, 14.0, -15.0], "pole": [32.0, 12.0, -25.0]},
                "arm_r": {"hand": [-40.0, 30.0, -5.0], "pole": [-45.0, 12.0, -25.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0.3, 0, 0],
                "wrist_out": [15.0, 0, 0]
            },
            "2": {
                # Lunge anticipation: forward shift, right claw wound up high
                "pelvis": {"loc": [0, 8.0, -2.5], "rot": [-6.0, 0, -15.0]},
                "spine": [-10.0, 0, -18.0],
                "head": [8.0, 0, 20.0],
                "arm_l": {"hand": [20.0, 15.0, -12.0], "pole": [45.0, 10.0, -30.0]},
                "arm_r": {"hand": [-25.0, -5.0, 22.0], "pole": [-55.0, -10.0, 5.0]},
                "leg_l": {"foot": [18.0, 22.0, -81.2], "pole": [20.0, 50.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 15.0, -40.0]},
                "fingers": [1.0, 0, 0],
                "wrist_out": [40.0, 0, 0]
            },
            "4": {
                # Impact: +18cm pelvis lunge! Brutal villain claw slap swipe!
                "pelvis": {"loc": [0, 18.0, -3.0], "rot": [-8.0, 0, 18.0]},
                "spine": [-12.0, 0, 24.0],
                "head": [8.0, 0, -10.0],
                "arm_l": {"hand": [25.0, -5.0, -15.0], "pole": [50.0, -15.0, -25.0]},
                "arm_r": {"hand": [-10.0, 48.0, 5.0], "pole": [-40.0, 20.0, -10.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [1.0, 0, 0],
                "wrist_out": [45.0, 0, 0]
            },
            "6": {
                # Followthrough
                "pelvis": {"loc": [0, 18.0, -2.5], "rot": [-7.0, 0, 22.0]},
                "spine": [-10.0, 0, 26.0],
                "head": [6.0, 0, -12.0],
                "arm_l": {"hand": [26.0, -8.0, -15.0], "pole": [52.0, -18.0, -25.0]},
                "arm_r": {"hand": [5.0, 46.0, -2.0], "pole": [-32.0, 18.0, -15.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [1.0, 0, 0],
                "wrist_out": [40.0, 0, 0]
            },
            "8": {
                # Moving Hold entry: head locks onto the claw!
                "pelvis": {"loc": [0, 17.5, -2.0], "rot": [-6.0, 0, 20.0]},
                "spine": [-8.0, 0, 22.0],
                "head": [-8.0, 0, 32.0],  # Stare intensely at hand
                "arm_l": {"hand": [22.0, -2.0, -18.0], "pole": [48.0, -10.0, -28.0]},
                "arm_r": {"hand": [2.0, 42.0, 4.0], "pole": [-28.0, 18.0, -12.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [1.0, 0, 0],
                "wrist_out": [38.0, 0, 0]
            },
            "14": {
                # Moving hold apex: staring into hand
                "pelvis": {"loc": [0, 17.0, -1.8], "rot": [-5.0, 0, 18.0]},
                "spine": [-7.0, 0, 20.0],
                "head": [-10.0, 0, 35.0], # Menacing gaze locked into fingers
                "arm_l": {"hand": [20.0, 0.0, -20.0], "pole": [45.0, -8.0, -30.0]},
                "arm_r": {"hand": [1.0, 40.0, 5.0], "pole": [-26.0, 18.0, -10.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [0.95, 0, 0],
                "wrist_out": [36.0, 0, 0]
            },
            # --- Tremor micro-oscillations (16..24) ---
            "16": {
                "pelvis": {"loc": [0, 16.0, -1.5], "rot": [-4.0, 0, 16.0]},
                "spine": [-6.0, 0, 17.0],
                "head": [-8.0, 0, 30.0],
                "arm_l": {"hand": [18.0, 5.0, -20.0], "pole": [42.0, -5.0, -30.0]},
                "arm_r": {"hand": [0.0, 38.5, 5.8], "pole": [-28.0, 16.0, -12.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [0.9, 0, 0],
                "wrist_out": [35.0, 0, 0]
            },
            "17": {
                "pelvis": {"loc": [0, 15.0, -1.4], "rot": [-4.0, 0, 15.0]},
                "spine": [-6.0, 0, 16.0],
                "head": [-7.0, 0, 28.0],
                "arm_l": {"hand": [18.0, 6.0, -20.0], "pole": [42.0, -5.0, -30.0]},
                "arm_r": {"hand": [2.0, 37.5, 4.2], "pole": [-28.0, 16.0, -12.0]},
                "leg_l": {"foot": [18.0, 30.0, -81.2], "pole": [20.0, 60.0, -35.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 10.0, -42.0]},
                "fingers": [0.85, 0, 0],
                "wrist_out": [28.0, 0, 0]
            },
            "18": {
                "pelvis": {"loc": [0, 14.0, -1.3], "rot": [-3.5, 0, 14.0]},
                "spine": [-5.5, 0, 15.0],
                "head": [-6.0, 0, 26.0],
                "arm_l": {"hand": [17.0, 7.0, -19.0], "pole": [41.0, -4.0, -30.0]},
                "arm_r": {"hand": [-1.0, 36.8, 5.2], "pole": [-29.0, 15.0, -13.0]},
                "leg_l": {"foot": [18.0, 28.0, -81.2], "pole": [20.0, 58.0, -36.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 12.0, -41.0]},
                "fingers": [0.8, 0, 0],
                "wrist_out": [32.0, 0, 0]
            },
            "19": {
                "pelvis": {"loc": [0, 13.0, -1.2], "rot": [-3.5, 0, 13.0]},
                "spine": [-5.5, 0, 13.5],
                "head": [-5.0, 0, 23.0],
                "arm_l": {"hand": [17.0, 8.0, -19.0], "pole": [41.0, -3.0, -30.0]},
                "arm_r": {"hand": [1.0, 35.5, 4.4], "pole": [-30.0, 15.0, -14.0]},
                "leg_l": {"foot": [18.0, 26.0, -81.2], "pole": [20.0, 55.0, -37.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 14.0, -41.0]},
                "fingers": [0.75, 0, 0],
                "wrist_out": [27.0, 0, 0]
            },
            "20": {
                "pelvis": {"loc": [0, 12.0, -1.0], "rot": [-3.0, 0, 12.0]},
                "spine": [-5.0, 0, 12.0],
                "head": [-4.0, 0, 20.0],
                "arm_l": {"hand": [16.0, 10.0, -18.0], "pole": [40.0, 0.0, -30.0]},
                "arm_r": {"hand": [-3.0, 34.8, 4.8], "pole": [-34.0, 14.0, -16.0]},
                "leg_l": {"foot": [18.0, 24.0, -81.2], "pole": [20.0, 52.0, -38.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 16.0, -40.0]},
                "fingers": [0.65, 0, 0],
                "wrist_out": [25.0, 0, 0]
            },
            "22": {
                "pelvis": {"loc": [0, 9.0, -0.8], "rot": [-2.5, 0, 9.0]},
                "spine": [-4.5, 0, 9.0],
                "head": [-1.0, 0, 14.0],
                "arm_l": {"hand": [16.0, 13.0, -15.0], "pole": [41.0, 2.0, -30.0]},
                "arm_r": {"hand": [-8.0, 32.0, 1.0], "pole": [-36.0, 13.0, -20.0]},
                "leg_l": {"foot": [18.0, 20.0, -81.2], "pole": [20.0, 48.0, -39.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 18.0, -40.0]},
                "fingers": [0.5, 0, 0],
                "wrist_out": [18.0, 0, 0]
            },
            "24": {
                # Tremor fully decayed, body weight pulling back to center
                "pelvis": {"loc": [0, 6.0, -0.5], "rot": [-2.0, 0, 6.0]},
                "spine": [-4.0, 0, 6.0],
                "head": [2.0, 0, 8.0],
                "arm_l": {"hand": [16.0, 16.0, -12.0], "pole": [42.0, 5.0, -30.0]},
                "arm_r": {"hand": [-14.0, 28.0, -4.0], "pole": [-40.0, 12.0, -24.0]},
                "leg_l": {"foot": [18.0, 16.0, -81.2], "pole": [20.0, 44.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 19.0, -40.0]},
                "fingers": [0.3, 0, 0],
                "wrist_out": [10.0, 0, 0]
            },
            "30": {
                # Neutral guard
                "pelvis": {"loc": [0, 0, 0], "rot": [0, 0, 0]},
                "spine": [-5.0, 0, 0],
                "head": [5.0, 0, 0],
                "arm_l": {"hand": [20.0, 22.0, -10.0], "pole": [45.0, 10.0, -30.0]},
                "arm_r": {"hand": [-20.0, 25.0, -12.0], "pole": [-45.0, 10.0, -30.0]},
                "leg_l": {"foot": [18.0, 12.0, -81.2], "pole": [20.0, 40.0, -40.0]},
                "leg_r": {"foot": [-18.0, -15.0, -81.2], "pole": [-20.0, 20.0, -40.0]},
                "fingers": [0, 0, 0],
                "wrist_out": [0, 0, 0]
            }
        }
    }
    with open(JAB2_CARD, "w", encoding="utf-8") as f:
        json.dump(c2, f, indent=1)
    print("Updated overlord_jab2.json with author block")

if __name__ == "__main__":
    setup_cards()
