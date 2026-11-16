"""python Saved/AnimR2/run_capture.py <kit> <out_dir> [sequences]   (editor already running with L_Test_Hub open)
Starts Tools/GASNPPTest/pie_capture_move.py inside it (returns at once); the capture ends with done.txt in <out_dir> and QUIT_EDITOR."""
import os, sys
P = 'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59'
kit, out = sys.argv[1], os.path.abspath(sys.argv[2]).replace(chr(92), '/')
seqs = sys.argv[3] if len(sys.argv) > 3 else ''
boot = ("import os\nos.environ['PIE_CAPTURE_KIT']=%r\nos.environ['PIE_CAPTURE_OUT']=%r\n" % (kit, out)
        + ("os.environ['PIE_CAPTURE_BUTTON']=%r\n" % os.environ['PIE_CAPTURE_BUTTON'] if os.environ.get('PIE_CAPTURE_BUTTON') else '')
        + ("os.environ['PIE_CAPTURE_SEQUENCES']=%r\n" % seqs if seqs else '')
        + "exec(open(%r).read())\n" % (P + '/Tools/GASNPPTest/pie_capture_move.py'))
os.makedirs(out, exist_ok=True)
open(out + '/boot.py', 'w').write(boot)
sys.argv = ['x', out + '/boot.py', out + '/boot.log']
exec(open(P + '/Saved/AnimR2/run_in_editor.py').read())
