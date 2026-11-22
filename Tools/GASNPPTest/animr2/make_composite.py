"""python Saved/AnimR2/make_composite.py <capture_dir> <sequence> <out.png> : game camera (behind) + side (az90) rows for chosen video frames"""
import sys; sys.path.insert(0, 'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Tools/GASNPPTest')
import pie_capture_grid as G
from PIL import Image
cap, seq, out = sys.argv[1:4]
rows = {r['f']: r for r in G.rows_of(cap, seq)}
frames = [39, 40, 41, 42, 43, 44, 51, 52, 53, 54, 60, 66, 72, 78, 86, 92, 98, 104, 110, 115]
cols, size = 10, 200
sheet = Image.new('RGB', (cols * size, 4 * size))
for vi, view in enumerate(('game', 'az90')):
    for i, f in enumerate(frames):
        sheet.paste(G.cell(cap, seq, view, rows[f], size), ((i % cols) * size, (vi * 2 + i // cols) * size))
sheet.save(out)
