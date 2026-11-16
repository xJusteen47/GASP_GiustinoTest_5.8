import json,re,csv,sys
d=json.load(open(sys.argv[1]));A=d['assets'];R=d['refby'];T=d['txt']
G='/Game/GSTN';TS='/Game/GSTNTests';EX='/Game/GSTNExamples';AR='/Game/GSTN/_Archive'
rows=[]
def move(p):
    n=p.rsplit('/',1)[1]; f=p.rsplit('/',1)[0]
    st='sposta';note=''
    if f=='/Game/GASNPPTest/Combat/Overlord':
        m=re.search(r'(DAir|Dropkick|Jab1|Jab2)',n); mv=m.group(1)
        if re.search(r'DAirV[78]|Jab[12]V2',n): return AR+'/Overlord/'+n,'archivio','versione superata (V7/V8 DAir, Jab V2); niente la usa a parte la sua catena'
        return G+'/Characters/Overlord/Moves/'+mv+'/'+n,st,''
    if f=='/Game/GASNPPTest/Combat':
        if re.match(r'(KIT|WPN)_Test_Overlord$',n): return G+'/Characters/Overlord/'+n,st,'rinomina proposta: '+n.replace('_Test','')
        if re.match(r'(KIT|WPN)_Test_OverlordDAirV[78]',n): return AR+'/Overlord/'+n,'archivio','kit di una versione superata'
        if re.match(r'(KIT|WPN)_Test_Overlord',n): return G+'/Characters/Overlord/Variants/'+n,st,'kit di prova di una variante'
        for pre,sub in [('KIT_','Kits'),('WPN_','Weapons'),('SUP_','Supers'),('MOD_','Mods'),('HBS_','Hurtboxes'),('PS_','Hurtboxes'),('TL_','Timelines'),('SIS_','Supers'),('BPK_','Backpacks'),('PM_','Surfaces'),('L_','')]:
            if n.startswith(pre): return (TS+'/Combat/'+sub+'/'+n) if sub else TS+'/Maps/'+n,st,''
        if n.startswith('BP_') and n.endswith('_Example'): return EX+'/CombatSeams/'+n,st,''
        if n=='GSTN_CombatRules_TestParts': return TS+'/Combat/Rules/'+n,st,'rinomina proposta: CR_Test_Parts'
        if n=='DA_Locomotion_Default': return G+'/Movement/'+n,st,'usato da tutti i kit di test'
    if f=='/Game/GASNPPTest/Combat/Mods': return TS+'/Combat/Mods/'+n,st,''
    if f=='/Game/GASNPPTest/Combat/AirCrouch': return G+'/Animation/Placeholders/'+n,st,'PLACEHOLDER'
    if f=='/Game/GASNPPTest/Combat/Archetypes': return TS+'/Combat/Archetypes/'+n.split('_')[1]+'/'+n,st,''
    if f=='/Game/GASNPPTest/Combat/Reaction':
        if n=='AM_React_GroundHit': return AR+'/Reactions/'+n,'archivio','nessun riferimento (asset, codice, ini)'
        return G+'/Combat/Reactions/'+n,st,''
    if f=='/Game/GASNPPTest':
        return {'BP_GASNPP_TestCharacter':G+'/Core/','GM_GASNPPTest':G+'/Core/','BP_Test_BouncePad':G+'/LevelObjects/'}[n]+n,st,'rinomina proposta in fase 3' if 'GASNPP' in n else ''
    if f=='/Game/GASNPPTest/Input': return G+'/Input/'+('Actions/' if n.startswith('IA_') else '')+n,st,''
    if f=='/Game/GAS/Input': return G+'/Input/'+n,st,''
    if f=='/Game/GAS/Backpacks': return G+'/Backpacks/'+n,st,''
    if f=='/Game/GASNPPTest/Stance': return TS+'/Combat/Stances/'+n,st,''
    if f.startswith('/Game/GASNPPTest/Examples'): return EX+f[len('/Game/GASNPPTest/Examples'):]+'/'+n,st,'vedi decisione 1 (esempi nel plugin)'
    if f=='/Game/GASNPPTest/TestMaps/Hub': return G+'/Maps/Hub/Materials/'+n,st,''
    if f=='/Game/Levels/Test': return G+'/Maps/'+n,st,'mappa di playtest; ini EditorStartupMap/GameDefaultMap'
    if p in('/Game/Levels/L_WallSlideAB','/Game/Levels/DefaultLevel_Harness'): return TS+'/Maps/'+n,st,''
    if f=='/Game/Blueprints/Animation' and n in('ABP_GSTN_WallLayer','ABP_GSTN_WallLayer_Heavy','ALI_GSTN_Wall','DA_AnimSet_Wall','DA_AnimSet_Wall_Heavy','DA_AnimSetCollection_SandboxCharacter'):
        return G+'/Animation/Layers/'+n,st,''
    if f=='/Game/Blueprints/DepthMarker': return G+'/DepthMarker/'+n,st,'ini DefaultGSTNDepthMarker'
    if f=='/Game/Blueprints/Equipment': return G+'/Equipment/'+n,st,''
    if f=='/GSTNParts/Samples': return '/GSTNParts/Examples/'+n,st,'nome cartella uniforme agli altri plugin'
    if f.startswith('/Game/AnimPipeline'): return p,'resta','sorgente pipeline anim (Live Link, script); doppione voluto della copia di gioco in Characters/Overlord/Moves'
    if p=='/Game/Blueprints/NewBlueprint': return p,'da verificare','Actor vuoto piazzato in DefaultLevel (GASP): probabilmente spazzatura; non spostato'
    if f.startswith('/GSTN'): return p,'resta','contenuto plugin (Rules = shipping; Test = fixture dei test C++)'
    return None
for p in sorted(A):
    r=move(p)
    if not r: continue
    new,st,note=r
    if '/Overlord/Moves/' in new and '/AS_OverlordDropkick' in new: note=(note+'; ' if note else '')+'doppione di /Game/AnimPipeline/Rig/... (sorgente)'
    rows.append([p,new,st,len(R.get(p,[])),len(T.get(p,[])),note])
w=csv.writer(open(sys.argv[2],'w',newline=''));w.writerow(['vecchio_percorso','nuovo_percorso','stato','ref_da_asset','ref_da_codice_ini','nota']);w.writerows(rows)
import collections;print(collections.Counter(r[2] for r in rows), len(rows))
print(collections.Counter(r[1].rsplit('/',1)[0] for r in rows if r[2]!='resta').most_common(40))
