# camera-p1a-pitch (2026-11-21): reads the P1a camera look log (gstn.Camera.LookLog 1 -> Saved/Logs/gstn_camera_look_<pawn>.csv) and says
# whether the camera still feeds its own motion back as look input (the pitch lock: orbit pitch running to the Free clamp -70 / +20).
# Usage: python camera_look_check.py [csv]   (default: newest Saved/Logs/gstn_camera_look_*.csv). Prints max 5 lines; details in <csv>.check.txt
# PASS = (1) on frames with no look input the orbit pitch does not move (no integrator), (2) the pitch never sits on a clamp for more than
# 0.5 s without look input, (3) the frames with look input (stick) move the orbit pitch the same way as the input.
import csv, glob, os, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def main():
    if len(sys.argv) > 1:
        path = sys.argv[1]
    else:
        files = glob.glob(os.path.join(ROOT, 'Saved', 'Logs', 'gstn_camera_look_*.csv'))
        if not files:
            print('ESITO: NO DATA (no Saved/Logs/gstn_camera_look_*.csv: was gstn.Camera.P1a 1 and gstn.Camera.LookLog 1 on?)')
            return 2
        path = max(files, key=os.path.getmtime)
    with open(path, newline='') as f:
        rows = [r for r in csv.DictReader(f) if r.get('frame', '').isdigit()]
    if len(rows) < 30:
        print(f'ESITO: NO DATA ({len(rows)} rows in {path})')
        return 2
    num = lambda r, k: float(r[k])
    still_moves, clamp_run, clamp_worst, input_rows, input_wrong, lines = 0, 0.0, 0.0, 0, 0, []
    for prev, cur in zip(rows, rows[1:]):
        dt = max(0.0, num(cur, 'time') - num(prev, 'time'))
        look = abs(num(cur, 'look_pitch_deg')) + abs(num(cur, 'look_yaw_deg'))
        dpitch = num(cur, 'orbit_pitch') - num(prev, 'orbit_pitch')
        at_clamp = min(abs(num(cur, 'orbit_pitch') - num(cur, 'pitch_min')), abs(num(cur, 'orbit_pitch') - num(cur, 'pitch_max'))) < 0.05
        if look < 1e-3:
            if abs(dpitch) > 0.05:
                still_moves += 1
                lines.append(f'self-move frame {cur["frame"]}: orbit pitch {num(prev, "orbit_pitch"):.2f} -> {num(cur, "orbit_pitch"):.2f} with no look input')
            clamp_run = clamp_run + dt if at_clamp else 0.0
            clamp_worst = max(clamp_worst, clamp_run)
        else:
            clamp_run = 0.0
            input_rows += 1
            lp = num(cur, 'look_pitch_deg')
            if abs(lp) > 0.05 and not at_clamp and dpitch * lp < 0:
                input_wrong += 1
                lines.append(f'wrong way frame {cur["frame"]}: look pitch {lp:.2f}, orbit pitch moved {dpitch:.2f}')
    pitches = sorted(num(r, 'orbit_pitch') for r in rows)
    ok = still_moves == 0 and clamp_worst <= 0.5 and input_wrong == 0
    with open(path + '.check.txt', 'w') as f:
        f.write('\n'.join(lines[:500]) + '\n')
    print(f'rows {len(rows)} | orbit pitch min {pitches[0]:.1f} median {pitches[len(pitches) // 2]:.1f} max {pitches[-1]:.1f} | source {rows[-1]["source"]} (1 = ControlRotation)')
    print(f'(1) frames with no look input where the pitch moved: {still_moves} (want 0)')
    print(f'(2) longest time on a pitch clamp without look input: {clamp_worst:.2f} s (want <= 0.5)')
    print(f'(3) frames with look input: {input_rows}, moved the wrong way: {input_wrong} (want 0; 0 inputs = stick not tested)')
    print(f'ESITO: {"PASS" if ok else "FAIL"} ({os.path.basename(path)}, details in .check.txt)')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
