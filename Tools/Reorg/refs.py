import os,re,sys,json,collections
root=sys.argv[1]; out=sys.argv[2]
def pkg(p):
    p=p.replace(os.sep,'/')
    if p.startswith('Content/'): return '/Game/'+p[8:].rsplit('.',1)[0]
    m=re.match(r'Plugins/([^/]+)/Content/(.*)',p)
    return '/'+m.group(1)+'/'+m.group(2).rsplit('.',1)[0]
assets={}
for base in ['Content']+['Plugins/'+d+'/Content' for d in os.listdir('Plugins')]:
    for dp,_,fs in os.walk(os.path.join(root,base)):
        for f in fs:
            if f.endswith(('.uasset','.umap')):
                rel=os.path.relpath(os.path.join(dp,f),root); assets[pkg(rel)]=rel
rx=re.compile(rb'/(?:Game|GSTN[A-Za-z]+)/[A-Za-z0-9_/]+')
refby=collections.defaultdict(set)
for p,rel in assets.items():
    data=open(os.path.join(root,rel),'rb').read()
    if data[:4]!=b'\xc1\x83\x2a\x9e': continue
    for m in set(rx.findall(data)):
        s=m.decode(); 
        if s in assets and s!=p: refby[s].add(p)
# text refs
txt=collections.defaultdict(set)
trx=re.compile(r'/(?:Game|GSTN[A-Za-z]+)/[A-Za-z0-9_/]+')
for dp,ds,fs in os.walk(root):
    ds[:]=[d for d in ds if d not in('.git','Content','UE_MCP_Bridge','Artifacts','docs')]
    for f in fs:
        if f.endswith(('.py','.cpp','.h','.ini','.ps1','.json','.cs')):
            fp=os.path.join(dp,f)
            try: t=open(fp,encoding='utf-8',errors='ignore').read()
            except: continue
            for m in set(trx.findall(t)):
                if m in assets: txt[m].add(os.path.relpath(fp,root))
json.dump({'assets':assets,'refby':{k:sorted(v) for k,v in refby.items()},'txt':{k:sorted(v) for k,v in txt.items()}},open(out,'w'),indent=0)
un=[p for p in assets if not refby.get(p) and not txt.get(p)]
print(len(assets),'assets;',len(un),'with no asset/code ref')
