import os, sys
TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import ue

def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "GanonFTiltV31_Build_VarD"
    script_path = os.path.join(TOOLS, "ue_open_sequencer.py").replace("\\", "/")
    res = ue.call("run_python_file", {
        "filePath": script_path,
        "entryPoint": "run",
        "kwargs": {"name": name}
    })
    print("Result:", res)

if __name__ == "__main__":
    main()
