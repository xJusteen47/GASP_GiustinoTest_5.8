import re, sys
def split_top(s):
    out=[];d=0;cur="";q=False
    for ch in s:
        if ch=='"' : q=not q
        if not q:
            if ch=="(": d+=1
            elif ch==")": d-=1
        if ch=="," and d==0 and not q: out.append(cur);cur=""
        else: cur+=ch
    if cur.strip(): out.append(cur)
    return out
def field(s,name):
    i=s.find(name+": ")
    if i<0: return ""
    j=i+len(name)+2
    if s[j]=="(":
        d=0
        for k in range(j,len(s)):
            if s[k]=="(":d+=1
            elif s[k]==")":
                d-=1
                if d==0: return s[j+1:k]
    m=re.match(r'"([^"]*)"',s[j:]); return m.group(1) if m else ""
def clean(x):
    x=x.replace('\\"','"').replace("\'","'")
    x=re.sub(r"/Script/[A-Za-z]+\.[A-Za-z_]+'(/[^']+)'",lambda m:m.group(1).split(".")[-1].split(":")[-1],x)
    x=re.sub(r"/Script/Chooser\.","",x); x=re.sub(r"/Script/[A-Za-z]+\.","",x)
    return x
src=open(sys.argv[1],encoding="utf-8").read().splitlines()
out=[]
for i,l in enumerate(src):
    if l.startswith("=== "):
        out.append("\n"+l.replace("/Game/Characters/UEFN_Mannequin/Animations/","…/")); continue
    if not l.startswith("<Struct"): out.append(l[:300]); continue
    l=clean(l)
    params=field(l,"parameters"); out.append(" params: "+params[:400])
    cols=split_top(field(l,"columns"))
    for ci,c in enumerate(cols):
        t=re.search(r'Type="?([A-Za-z_]+)',c); pc=re.search(r'PropertyChain=\(([^)]*)\)',c)
        extra="" 
        if "Output" in c or "PoseMatch" in c or "Randomize" in c or not pc: extra=" "+c[:260]
        out.append(" col%d %s %s%s"%(ci,t.group(1) if t else "?",pc.group(1) if pc else "",extra))
    rows=split_top(field(l,"rows"))
    out.append(" rows: %d  fallback: %s"%(len(rows),field(l,"fallback")))
    for ri,r in enumerate(rows):
        res=re.search(r'Result="\((\w+)=([^)]*)\)',r)
        rs=(res.group(2) if res else r[:80]).strip('"').split(":")[-1]
        cells=re.search(r"Cells=\((.*)\)\)?$",r)
        cs=cells.group(1) if cells else ""
        cs=re.sub(r"\(Comparison=MatchEqual,Value=","(",cs)
        dis=" DISABLED" if "Disabled=True" in r else ""
        out.append("  r%d%s %s | %s"%(ri,dis,rs,cs[:420]))
open(sys.argv[2],"w",encoding="utf-8").write("\n".join(out))
