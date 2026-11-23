import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
JAB1 = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab1.json")
JAB2 = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab2.json")

# Jab 1
with open(JAB1, "r", encoding="utf-8") as f:
    c1 = json.load(f)

c1["author"]["frames"] = 14
# Poses in 0..13
c1["poses"] = [
    {"name": "guard", "frame": 0, "source": None},
    {"name": "anticipation_load", "frame": 2, "source": None, "spacing": "charge_long"},
    {"name": "strike_contact", "frame": 4, "source": None, "spacing": "arrive_1_2"},
    {"name": "followthrough", "frame": 6, "source": None, "spacing": "hold_recoil"},
    {"name": "recoil_cancel", "frame": 8, "source": None, "spacing": "limb_led_recovery"},
    {"name": "recovery_settle", "frame": 11, "source": None, "spacing": "arrive_1_2"},
    {"name": "neutral_guard", "frame": 13, "source": None}
]

with open(JAB1, "w", encoding="utf-8") as f:
    json.dump(c1, f, indent=1)

# Jab 2
with open(JAB2, "r", encoding="utf-8") as f:
    c2 = json.load(f)

c2["author"]["frames"] = 32
c2["author"]["poses"]["31"] = dict(c2["author"]["poses"]["30"])
del c2["author"]["poses"]["30"]

c2["poses"] = [
    {"name": "entry_lunge_start", "frame": 0, "source": None},
    {"name": "lunge_anticipation", "frame": 2, "source": None, "spacing": "charge_long"},
    {"name": "strike_contact", "frame": 4, "source": None, "spacing": "arrive_1_2"},
    {"name": "followthrough_apex", "frame": 6, "source": None, "spacing": "hold_recoil"},
    {"name": "stare_hand_start", "frame": 8, "source": None, "spacing": "hold_recoil"},
    {"name": "moving_hold_apex", "frame": 14, "source": None, "spacing": "hold_recoil"},
    {"name": "tremor_start", "frame": 16, "source": None},
    {"name": "tremor_osc1", "frame": 17, "source": None},
    {"name": "tremor_osc2", "frame": 18, "source": None},
    {"name": "tremor_osc3", "frame": 19, "source": None},
    {"name": "tremor_mid", "frame": 20, "source": None},
    {"name": "tremor_decay", "frame": 22, "source": None},
    {"name": "tremor_end", "frame": 24, "source": None, "spacing": "limb_led_recovery"},
    {"name": "neutral_guard", "frame": 31, "source": None}
]

with open(JAB2, "w", encoding="utf-8") as f:
    json.dump(c2, f, indent=1)

print("Synchronized card poses and author frames.")
