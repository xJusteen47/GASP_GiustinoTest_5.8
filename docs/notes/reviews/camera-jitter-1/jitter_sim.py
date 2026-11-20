# Camera anchor tick-order model. Sim 62.5 Hz, render ~120 fps with +-15% frame time noise.
# Jump: v0 900 cm/s up, g -2400, falls to -1500 terminal (dropkick-like). Air hold on: anchor goal = ground (0).
import math, random
random.seed(1)
SIM=1/62.5
def simz(n):  # sim state at sim frame n
    t=n*SIM; v0=900; g=-2400
    z=v0*t+0.5*g*t*t
    return max(z,0.0) if t<1.5 else 0.0
def run(fps, early):
    t=0; prev_damped=0.0; prev_root=0.0; errs=[]
    while t<1.2:
        t+=1/fps*(1+random.uniform(-.15,.15))
        n=int(t/SIM)                      # latest sim frame
        root=simz(n)                      # actor root steps at sim rate
        a=(t-n*SIM)/SIM
        mesh=simz(n-1)+(simz(n)-simz(n-1))*a   # smoothed mesh (1 sim frame behind)
        damped=0.0                        # air hold: camera height target stays on the ground
        cam = damped if early else root+(prev_damped-prev_root)  # old: correction written after the view, root moved since
        errs.append(cam-damped)
        prev_damped,prev_root=damped,root
    return max(abs(e) for e in errs), sum(1 for e in errs if abs(e)>2)/len(errs)
for fps in (60,120,144):
    for early in (False,True):
        m,f=run(fps,early); print(f"fps {fps} {'new' if early else 'old'}: camera off its target max {m:.1f} cm, frames >2cm {f*100:.0f}%")
