#!/bin/bash
# cap_one.sh <kit> <out_dir> <button> [sequences]
P="C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59"; cd "$P"
BOOT="C:/Users/jacop/AppData/Local/Temp/claude/8c81644554604783/a744393f-c912-4667-b073-01ffe1248c78/scratchpad/boot_vd2.py"
rm -rf "$2"; mkdir -p "$2"
powershell -NoProfile -Command "Start-Process 'C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59\GASP_Giustino2.uproject\"','/Game/Levels/Test/L_Test_Hub','-ExecCmds=\"py $BOOT\"','-nosplash'" 
for i in $(seq 1 60); do c=$(curl -s -m3 -o /dev/null -w "%{http_code}" http://127.0.0.1:8000/mcp); [ "$c" = 405 ] && break; sleep 5; done
echo mcp=$c; sleep 45
export PIE_CAPTURE_BUTTON="$3"
python Saved/AnimR2/run_capture.py "$1" "$2" $4
for i in $(seq 1 80); do [ -f "$2/done.txt" ] && break; sleep 5; done
cat "$2/done.txt"; sleep 8; tasklist | grep -c -i UnrealEditor
