import sys,json
sys.path.insert(0,'.')
from animpipe import life as L, curves as CV
for name in ['JabV12','GanonFTiltV31']:
    F=L.load(f'../../docs/notes/life-calib-run/dumps/joints_AS_{name}_Build.json')
    pts=['pelvis']+L._pts(F)
    print('==',name,len(F),'pts',pts)
    S={p:L._speed(F,p) for p in pts}
    steps=CV.body_steps(F,L._pts(F))
    print('step '+' '.join(f'{p[:6]:>6}' for p in pts)+'  body')
    for i in range(len(F)-1):
        print(f'{i:2d}-{i+1:2d} '+' '.join(f'{S[p][i]:6.1f}' for p in pts)+f'  {steps[i][0]:5.1f} {steps[i][1]}')
    r=L.run(F)
    print(json.dumps(r['block']))
