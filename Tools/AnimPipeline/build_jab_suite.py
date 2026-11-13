"""Build script for Overlord 2-Hit Jab suite.

Builds:
  1. AS_OverlordJab1 (Hit 1: Backhand slap with opposite arm folded across torso)
  2. AS_OverlordJab2 (Hit 2: Forward lunge with villain claw slap and held stare)
  3. AM_OverlordJab & LS_OverlordJab_Preview (Chained preview sequence open in Sequencer)
"""
import os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

def run():
    import ue
    from animpipe.__main__ import build
    
    print("=== 1/3: Building Overlord Jab 1 ===")
    card1_path = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab1.json")
    build(card1_path)
    
    print("\n=== 2/3: Building Overlord Jab 2 ===")
    card2_path = os.path.join(HERE, "cards", "overlord", "jab", "overlord_jab2.json")
    build(card2_path)
    
    print("\n=== 3/3: Assembling Preview LevelSequence & Montage in Sequencer ===")
    r = ue.call("run_python_file", {
        "filePath": os.path.join(HERE, "assemble_jab_preview.py").replace("\\", "/"),
        "entryPoint": "run",
        "kwargs": {}
    })
    print("Assemble result:", r)
    print("\n=== All builds completed successfully! ===")

if __name__ == "__main__":
    try:
        run()
    except Exception as e:
        print("Build failed:", e)
        sys.exit(1)
