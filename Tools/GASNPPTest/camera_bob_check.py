import csv,sys
def load(p): return {int(x['frame']):x for x in csv.DictReader(open(p))}
for name in sys.argv[1:]:
    r=load(f'Saved/CamShift/{name}/rows.csv')
    fs=sorted(f for f in r if 40<=f<=200)
    steps=[abs(float(r[f]['pov_z'])-float(r[p]['pov_z'])) for p,f in zip(fs,fs[1:])]
    pit=[abs(float(r[f]['pov_pitch'])-float(r[p]['pov_pitch'])) for p,f in zip(fs,fs[1:])]
    zs=[float(r[f]['pov_z']) for f in fs]
    # direction reversals of pov z after landing (f>=90) bigger than 2 cm
    seq=[float(r[f]['pov_z']) for f in fs if f>=90]
    rev=0;d0=0
    for a,b in zip(seq,seq[1:]):
        d=b-a
        if abs(d)>2:
            s=1 if d>0 else -1
            if d0 and s!=d0: rev+=1
            d0=s
    print(f'{name}: max cam z step/frame {max(steps):.1f} cm, max pitch step {max(pit):.2f} deg, cam z range {min(zs):.0f}-{max(zs):.0f}, reversals after landing {rev}')
