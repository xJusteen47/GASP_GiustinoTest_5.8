# Offline model of FGSTN_JumpLogic (Step + GravityScale + terminal speed) at 62.5 Hz, for the numbers in docs/notes/handovers/jump-hold-gravity.md.
# python Tools/GAS/jump_hold_sim.py -> apex cm, apex frame, air frames per kit; 'legacy' = CutUntilApex with the Mario 3D World values.
import sys
def sim(v0,rise,cut,fall,minh,maxh,hold,ramp=3,cap=1500,dt=1/62.5,g=980,model='hw'):
    v=v0;y=0;apex=0;af=0;cutf=None;over=False;f=0;rising=True;cutframes=0
    while True:
        f+=1
        el=f
        if rising:
            if model=='hw' and el>=maxh: over=True
            if cutf is None and (el>=maxh or (f>hold and el>=minh)): cutf=f;cutframes=0
            elif cutf is not None: cutframes+=1
            if v<=0: rising=False
        if not rising: s=fall
        else:
            tgt = fall if over else cut
            if cutf is None: s=rise
            elif ramp>0: s=rise+(tgt-rise)*min(1,(cutframes+1)/ramp)
            else: s=tgt
        step=g*s*dt
        down=-v
        if not rising and cap>0 and down+step>cap: step=max(0,cap-down)
        v-=step; y+=v*dt
        if y>apex: apex=y;af=f
        if y<=0 and f>2: return apex,af,f
for name,a in [('medium',(1350,2.6,9,9,0,30)),('light',(1540,2.6,7.9,7.9,0,30)),('heavy',(1170,2.6,11.6,11.6,0,30)),('3dw-legacy',(1240,0.01,4.1,4.1,2,11))]:
    m='legacy' if 'legacy' in name else 'hw'
    full=sim(*a,hold=999,model=m); tap=sim(*a,hold=0,model=m); mid=sim(*a,hold=10,model=m)
    print(name,'full apex %.0f cm f%d air %d f (%.2fs)'%(full[0],full[1],full[2],full[2]/62.5),'| tap %.0f'%tap[0],'| hold10 %.0f'%mid[0])
