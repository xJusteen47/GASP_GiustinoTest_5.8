import sys
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.worksheet.datavalidation import DataValidation
from openpyxl.formatting.rule import FormulaRule
from openpyxl.utils import get_column_letter as L

OUT = sys.argv[1]
wb = Workbook()

IN = PatternFill("solid", fgColor="FFF8DC")     # da compilare
CALC = PatternFill("solid", fgColor="E8E8E8")   # calcolato
HEAD = PatternFill("solid", fgColor="2F4F6F")
SEC = PatternFill("solid", fgColor="D6E4F0")
Q = PatternFill("solid", fgColor="FFD966")      # "?" = da confermare
HF = Font(bold=True, color="FFFFFF")
B = Font(bold=True)
thin = Side(style="thin", color="BBBBBB")
BOX = Border(left=thin, right=thin, top=thin, bottom=thin)
WRAP = Alignment(wrap_text=True, vertical="top")

# ---------------- Liste (dropdown) ----------------
lists = {
    "Stato": ["0 idea", "1 pose owner", "2 blocking", "3 in gioco", "4 testata", "5 polish", "6 buona", "7 approvata"],
    "Dove": ["Terra", "Aria", "Terra→Aria", "Aria→Terra", "Entrambi"],
    "Input": ["X", "X avanti", "X su", "X giù", "RT", "RT avanti", "RT su", "RT giù", "Y", "Y avanti", "Y su", "Y giù", "B", "LT+X", "LT+RT", "corsa+X", "corsa+RT", "altro"],
    "Peso": ["leggera", "media", "pesante"],
    "Atterraggio": ["n/a (terra)", "autocancel", "landing lag", "continua con mossa", "hold fino a terra"],
    "SiNo": ["sì", "no", "?"],
    "Ledge": ["n/a", "Stop", "EndMove", "Recoil", "ContinueWithMove"],
    "Capsula": ["standard", "traccia capsula", "accucciata", "?"],
    "Ingresso": ["Snap", "EaseOut", "EaseIn", "EaseInOut", "Bezier", "Lineare", "Stepped", "AI", "Interp"],
    "Forma": ["Sphere", "Capsule", "Box"],
    "Zona": ["Normal", "Sweetspot", "Sourspot", "Tipper", "Meteor", "Grab", "Windbox"],
    "Fase": ["Standby", "Windup", "Strike", "Follow-through", "Coda"],
    "Posa": ["Avvio", "Carica", "Breakdown", "Contatto", "Estremo", "Assestamento", "Ritorno", "Idle", "Loop", "Atterraggio", "altro"],
    "Chi": ["OWNER", "DATI", "?"],
    "Cancel": ["Always", "OnHit", "OnBlock", "OnWhiff", "Landing"],
}
ws_l = wb.active
ws_l.title = "Liste"
for c, (k, vals) in enumerate(lists.items(), 1):
    ws_l.cell(1, c, k).font = B
    for r, v in enumerate(vals, 2):
        ws_l.cell(r, c, v)
    ws_l.column_dimensions[L(c)].width = 16
ref = {k: f"Liste!${L(i)}$2:${L(i)}${len(v)+1}" for i, (k, v) in enumerate(lists.items(), 1)}


def dv(ws, key, rng):
    d = DataValidation(type="list", formula1=ref[key], allow_blank=True, showErrorMessage=False)
    ws.add_data_validation(d)
    d.add(rng)


def qmark(ws, rng):
    first = rng.split(":")[0]
    ws.conditional_formatting.add(rng, FormulaRule(formula=[f'ISNUMBER(SEARCH("?",{first}))'], fill=Q))


# ---------------- LEGGIMI ----------------
r = wb.create_sheet("LEGGIMI", 0)
txt = [
    ("Scheda moveset da compilare (v1, 2026-11-16)", True),
    ("", False),
    ("COSA COMPILI TU (owner)", True),
    ("1. Foglio Moveset: una riga per mossa. Bastano le colonne GIALLE fino a 'Tratto 1': il resto può restare vuoto o '?'.", False),
    ("2. Per ogni mossa: duplica il foglio Mossa_MODELLO (tasto destro sulla linguetta → Duplica), rinominalo Mossa_<ID>.", False),
    ("3. Nel dettaglio servono SUBITO solo: §1 tratti d'identità, §3 pose chiave (riferimento immagine o Cascadeur), §2 frame indicativi.", False),
    ("   Hitbox, curve, cancel li riempiamo noi in blocking; tu correggi.", False),
    ("", False),
    ("CONVENZIONI", True),
    ("Frame di GIOCO a 62,5 Hz (16 ms), il primo = 1, come i CSV pilota e la frame data di Smash. Il frame clip 60 fps si calcola da solo (grigio = round(f×0,96)).", False),
    ("Attenzione: la base 0/1 tra CSV e card è ancora '?' (schede-mosse-1, punto 7). La formula riproduce il dropkick (gioco 16 = clip 15).", False),
    ("'?' in una cella = da confermare: la cella diventa arancione e finisce nelle domande all'owner (regola 13d).", False),
    ("Chi: OWNER = deciso da te; DATI = letto dal kit/repo; ? = dedotto da noi.", False),
    ("Giallo = da compilare. Grigio = calcolato, non toccare. Le tendine sono nel foglio Liste (puoi aggiungere voci).", False),
    ("", False),
    ("FLUSSO (workflow 11-16)", True),
    ("0 idea → 1 pose owner → 2 blocking → 3 in gioco → 4 testata → 5 polish → 6 buona (tag good/) → 7 approvata (tag approved/, bloccata).", False),
    ("Prima tutte le mosse a 1, poi blocking di tutto il moveset (2-3), test con il pad (4), solo dopo polish (5).", False),
    ("", False),
    ("DA DOVE VIENE OGNI PARTE", True),
    ("Startup/Attivi/FAF: frame data di Smash, Street Fighter, Tekken (stesse tre fasi). Fasi Standby/Windup/Strike/Follow-through: Sakurai GDC 2008.", False),
    ("Finestre e hitbox su osso con raggio e offset: Rivals 2 Workshop. Tratti d'identità, pose chiave, curve: regola 13 del progetto.", False),
    ("Contesto/entrata/uscita/blend e lista con stato: brief di animazione standard (vedi ricerca.md).", False),
    ("", False),
    ("COME ARRIVA NEL GIOCO", True),
    ("Moveset → mosse.csv; §3 Pose → pose.csv; §5 Hitbox → hitbox.csv; §7 Cancel → cancel.csv (stesse colonne dei CSV pilota in anim-pipeline/pilota/).", False),
    ("§1-§4 → scheda docs/notes/mosse/<kit>-<mossa>.md (stesso ordine di _modello.md).", False),
]
for i, (t, bold) in enumerate(txt, 1):
    c = r.cell(i, 1, t)
    c.alignment = Alignment(wrap_text=True, vertical="top")
    if bold:
        c.font = Font(bold=True, size=13 if i == 1 else 11)
r.column_dimensions["A"].width = 95

# ---------------- Moveset ----------------
m = wb.create_sheet("Moveset", 1)
cols = [
    # (header, width, kind, list)
    ("ID", 8, "in", None),
    ("Kit", 10, "in", None),
    ("Mossa", 14, "in", None),
    ("Input", 10, "in", "Input"),
    ("Dove", 10, "in", "Dove"),
    ("Peso", 9, "in", "Peso"),
    ("Stato", 12, "in", "Stato"),
    ("Idea (1 riga)", 32, "in", None),
    ("Tratto 1", 22, "in", None),
    ("Tratto 2", 22, "in", None),
    ("Tratto 3", 22, "in", None),
    ("Riferimento (gioco/video)", 24, "in", None),
    ("Pose chiave (file/link)", 22, "in", None),
    ("Startup (1° attivo)", 9, "in", None),
    ("Ultimo attivo", 9, "in", None),
    ("FAF", 7, "in", None),
    ("Totale clip", 8, "in", None),
    ("Attivi", 7, "calc", None),
    ("Recovery", 8, "calc", None),
    ("Durata ms", 8, "calc", None),
    ("Danno %", 7, "in", None),
    ("Angolo °", 7, "in", None),
    ("KB base", 7, "in", None),
    ("KB crescita", 8, "in", None),
    ("Hitstop f", 7, "in", None),
    ("Atterraggio", 14, "in", "Atterraggio"),
    ("Landing lag f", 8, "in", None),
    ("Takeoff window", 9, "in", None),
    ("Max aria f", 8, "in", None),
    ("Ledge check", 11, "in", "Ledge"),
    ("Capsula", 12, "in", "Capsula"),
    ("Cancel verso", 18, "in", None),
    ("Priorità", 7, "in", None),
    ("Dettaglio", 14, "in", None),
    ("Note", 30, "in", None),
]
for c, (h, w, k, lst) in enumerate(cols, 1):
    cell = m.cell(1, c, h)
    cell.font, cell.fill, cell.border = HF, HEAD, BOX
    cell.alignment = Alignment(wrap_text=True, vertical="center", horizontal="center")
    m.column_dimensions[L(c)].width = w
m.row_dimensions[1].height = 42
idx = {h: c for c, (h, *_ ) in enumerate(cols, 1)}
N = 41  # rows 2..41
for row in range(2, N + 1):
    for c, (h, w, k, lst) in enumerate(cols, 1):
        cell = m.cell(row, c)
        cell.fill = CALC if k == "calc" else IN
        cell.border = BOX
        cell.alignment = WRAP
    S, E, F = (L(idx[x]) for x in ("Startup (1° attivo)", "Ultimo attivo", "FAF"))
    m.cell(row, idx["Attivi"], f'=IF(AND(ISNUMBER({S}{row}),ISNUMBER({E}{row})),{E}{row}-{S}{row}+1,"")')
    m.cell(row, idx["Recovery"], f'=IF(AND(ISNUMBER({F}{row}),ISNUMBER({E}{row})),{F}{row}-{E}{row}-1,"")')
    m.cell(row, idx["Durata ms"], f'=IF(ISNUMBER({F}{row}),{F}{row}*16,"")')
for h, (_, _, _, lst) in zip(idx, cols):
    if lst:
        col = L(idx[h])
        dv(m, lst, f"{col}2:{col}{N}")
qmark(m, f"A2:{L(len(cols))}{N}")
m.freeze_panes = "D2"
m.auto_filter.ref = f"A1:{L(len(cols))}{N}"

# example row: Overlord dropkick (from docs/notes/mosse/overlord-dropkick.md)
ex = {
    "ID": "OV-FAIR", "Kit": "Overlord", "Mossa": "Dropkick (Fair)", "Input": "X avanti", "Dove": "Aria→Terra",
    "Peso": "pesante", "Stato": "3 in gioco",
    "Idea (1 riga)": "Calcio volante di potere: si raccoglie, scatta a gambe unite e resta in aria colpendo fino a terra, poi slam e resta a terra",
    "Tratto 1": "Gambe unite e dritte (piedi ~10 cm)", "Tratto 2": "Braccia conserte sul petto (snap)",
    "Tratto 3": "Finisce GROUNDED (roll per combo)", "Riferimento (gioco/video)": "Big Band j.HK; refs Bison",
    "Pose chiave (file/link)": "base Antigravity r0 (good/dropkick-1)", "Startup (1° attivo)": 16,
    "Ultimo attivo": "? fino a terra", "FAF": "?", "Totale clip": "? 16+loop+32",
    "Danno %": "?", "Angolo °": "?", "KB base": "?", "KB crescita": "?", "Hitstop f": "?",
    "Atterraggio": "hold fino a terra", "Landing lag f": 33, "Takeoff window": "n/a", "Max aria f": 125,
    "Ledge check": "n/a", "Capsula": "?", "Cancel verso": "roll da a terra ?", "Priorità": 1,
    "Dettaglio": "Mossa_OV-FAIR",
    "Note": "Atterraggio 32 f clip (slam f1-2, rimbalzo f6, a terra f16). Rialzata non esiste ancora. Capsula: decisione 5 aperta.",
}
for h, v in ex.items():
    m.cell(2, idx[h], v)
m.cell(2, idx["Dettaglio"]).hyperlink = "#'Mossa_OV-FAIR'!A1"

# ---------------- Detail sheet builder ----------------
def section(ws, row, title, headers, widths_hint, nrows, lists_by_col=None, calc_cols=None, data=None):
    ws.cell(row, 1, title).font = Font(bold=True, size=12)
    for c in range(1, 10):
        ws.cell(row, c).fill = SEC
    row += 1
    for c, h in enumerate(headers, 1):
        cell = ws.cell(row, c, h)
        cell.font, cell.fill, cell.border = HF, HEAD, BOX
        cell.alignment = Alignment(wrap_text=True, vertical="center")
    hdr = row
    calc_cols = calc_cols or {}
    for i in range(nrows):
        rr = row + 1 + i
        for c in range(1, len(headers) + 1):
            cell = ws.cell(rr, c)
            cell.border, cell.alignment = BOX, WRAP
            cell.fill = CALC if c in calc_cols else IN
            if c in calc_cols:
                cell.value = calc_cols[c](rr)
        if data and i < len(data):
            for c, v in enumerate(data[i], 1):
                if v is not None and c not in calc_cols:
                    ws.cell(rr, c, v)
    for c, key in (lists_by_col or {}).items():
        col = L(c)
        dv(ws, key, f"{col}{hdr+1}:{col}{hdr+nrows}")
    qmark(ws, f"A{hdr+1}:{L(len(headers))}{hdr+nrows}")
    return row + nrows + 2


def detail(name, d):
    ws = wb.create_sheet(name)
    for c, w in enumerate([18, 14, 12, 12, 12, 12, 14, 14, 30], 1):
        ws.column_dimensions[L(c)].width = w
    ws.cell(1, 1, "Scheda mossa").font = Font(bold=True, size=14)
    ws.cell(1, 2, d.get("titolo", "<Kit> <Mossa>  (ID uguale al foglio Moveset)")).font = Font(bold=True, size=14)
    head = [("ID", d.get("id")), ("Versione di lavoro", d.get("ver")), ("Stato", d.get("stato")),
            ("Idea in 2 righe", d.get("idea")), ("Contesto: prima / dopo", d.get("ctx")),
            ("Entrata (da quale stato/posa)", d.get("in")), ("Uscita (verso quale stato/posa)", d.get("out"))]
    row = 3
    for k, v in head:
        ws.cell(row, 1, k).font = B
        ws.merge_cells(start_row=row, start_column=2, end_row=row, end_column=9)
        c = ws.cell(row, 2, v)
        c.fill, c.alignment = IN, WRAP
        row += 1
    dv(ws, "Stato", "B5")
    qmark(ws, "B3:B9")
    row += 1
    row = section(ws, row, "§1 IDENTITÀ (2-3 tratti; il revisore li controlla PER PRIMI; cambiarli = decisione owner)",
                  ["#", "Tratto", "", "", "Riferimento (immagine/video/posa)", "", "Chi", "Frame dove si vede", "Note"],
                  None, 3, {7: "Chi"}, data=d.get("tratti"))
    row = section(ws, row, "§2 FASI E FRAME (frame di GIOCO 62,5 Hz da 1; clip 60 fps calcolato)",
                  ["Fase", "Inizio f", "Fine f", "Clip60 inizio", "Clip60 fine", "Durata ms", "Tenuta f", "Chi", "Cosa deve leggersi"],
                  None, 6, {1: "Fase", 8: "Chi"},
                  {4: lambda r: f'=IF(ISNUMBER(B{r}),ROUND(B{r}*0.96,0),"")',
                   5: lambda r: f'=IF(ISNUMBER(C{r}),ROUND(C{r}*0.96,0),"")',
                   6: lambda r: f'=IF(AND(ISNUMBER(B{r}),ISNUMBER(C{r})),(C{r}-B{r}+1)*16,"")'},
                  data=d.get("fasi"))
    row = section(ws, row, "§3 POSE CHIAVE (le dai tu: immagine, file Cascadeur o 'come <gioco> f<n>') = pose.csv",
                  ["Posa", "Frame gioco", "Clip60", "Tenuta f", "Ingresso", "Smear", "Rif. immagine/file", "Chi", "Descrizione (cosa conta)"],
                  None, 8, {1: "Posa", 5: "Ingresso", 6: "SiNo", 8: "Chi"},
                  {3: lambda r: f'=IF(ISNUMBER(B{r}),ROUND(B{r}*0.96,0),"")'}, data=d.get("pose"))
    row = section(ws, row, "§4 FOGLIO CURVE (regola 13c: lo scrive chi anima, la base buona fa da base)",
                  ["Segmento (da→a)", "Frame", "Easing/spaziatura", "Overshoot", "Follow-through: parte", "Ritardo f", "Hold/settle", "Chi", "Note"],
                  None, 5, {8: "Chi"}, data=d.get("curve"))
    row = section(ws, row, "§5 HITBOX (= hitbox.csv; decisi dall'owner, i thread non li cambiano)",
                  ["Nome/zona", "Attivo da f", "a f", "Osso/socket", "Forma", "Raggio cm", "Danno %", "Angolo ° / yaw", "KB base / crescita / hitstun / hitstop"],
                  None, 4, {5: "Forma"}, data=d.get("hitbox"))
    row = section(ws, row, "§6 CORPO E MOVIMENTO (hurtbox, capsula, aria/terra, atterraggio, bordo)",
                  ["Voce", "Valore", "", "Frame", "", "", "", "Chi", "Note"],
                  None, 8, {8: "Chi"},
                  data=d.get("corpo") or [["Hurtbox speciale (armor/intang.)"], ["Capsula"], ["Dove (terra/aria)"],
                                          ["Takeoff window"], ["Atterraggio"], ["Landing lag f"], ["Max aria f"], ["Ledge check"]])
    row = section(ws, row, "§7 CANCEL (= cancel.csv)",
                  ["Verso (mossa/comando)", "Da f", "A f", "Condizione", "", "", "", "Chi", "Note"],
                  None, 3, {4: "Cancel", 8: "Chi"}, data=d.get("cancel"))
    row = section(ws, row, "§8 BLOCKING → POLISH (una riga per giro)",
                  ["Giro", "Data", "Stato", "Video/griglia", "Tratti OK?", "Esito", "Tag good/", "Chi", "Cosa manca"],
                  None, 4, {3: "Stato", 5: "SiNo", 8: "Chi"}, data=d.get("giri"))
    row = section(ws, row, "§9 DOMANDE APERTE (tutti i '?' qui sopra finiscono qui)",
                  ["#", "Domanda", "", "", "", "", "", "Per", "Opzioni / consigliata"],
                  None, 5, None, data=d.get("domande"))
    ws.freeze_panes = "A3"
    return ws


detail("Mossa_MODELLO", {})

dk = {
    "titolo": "Overlord Dropkick (Fair)",
    "id": "OV-FAIR", "ver": "base Antigravity r0 (ripristino in corso)", "stato": "3 in gioco",
    "idea": "Calcio volante di potere: raccolta in aria, scatto a gambe unite e braccia conserte, hitbox attiva finché tocca terra, slam con un rimbalzo, resta a terra.",
    "ctx": "Da salto o corsa+salto; dopo: a terra (grounded) da cui roll per estendere la combo.",
    "in": "Aria: idle aria / salto. Ingresso braccia conserte = snap durante la raccolta.",
    "out": "A terra (Movement grounded). Rialzata: non esiste ancora (?).",
    "tratti": [
        [1, "Gambe unite e dritte (piedi ~10 cm)", None, None, "base Antigravity r0", None, "OWNER", "16→fine loop", "owner 11-15"],
        [2, "Braccia conserte sul petto (posa di potere)", None, None, "base r0; anim-process Pilastro D; refs Bison", None, "OWNER", "dalla raccolta", "snap, parte da guardia"],
        [3, "Finisce GROUNDED (roll da lì)", None, None, "owner 11-15 15:31Z", None, "OWNER", "atterraggio f16+", "dettagli roll ?"],
    ],
    "fasi": [
        ["Windup", 1, 16, None, None, None, None, "DATI", "caduta → raccolta, chiavi clip 0,4,8,11,15"],
        ["Strike", 16, "? atterraggio", None, None, None, None, "OWNER", "hitbox attiva fino a terra; loop 12 f clip, respiro ±1,5 cm"],
        ["Follow-through", "? atterraggio", "+33", None, None, None, None, "DATI", "land 32 f clip: slam f1-2, rimbalzo f6, a terra f16, tenuta f31"],
        ["Coda", None, None, None, None, None, None, "?", "rialzata: veloce o animata?"],
    ],
    "pose": [
        ["Carica", 1, None, 0, "Snap", "no", "card startup f0", "DATI", "caduta, guardia"],
        ["Breakdown", 8, None, 0, "EaseOut", "no", "card startup f8", "DATI", "raccolta, braccia verso conserte"],
        ["Contatto", 16, None, 0, "EaseOut", "?", "card startup f15", "DATI", "gambe unite tese, braccia conserte: passo massimo 45,8 cm f14→15"],
        ["Loop", 16, None, 12, "Interp", "no", "card loop", "DATI", "tenuta in aria finché non tocca terra"],
        ["Atterraggio", None, None, 0, "Snap", "no", "card land f1", "DATI", "slam + 1 rimbalzo, testa frusta indietro"],
    ],
    "curve": [
        ["Startup", "16 clip", "rilascio 6/10/26/58% su f12-15", "n/d", "busto/testa/braccia", "1-2", None, "DATI", "card r1: valori base r0 ?"],
        ["Loop", "12 clip", "respiro lento", None, "busto/testa", "1-2", None, "DATI", None],
        ["Land", "32 clip", "slam + 1 rimbalzo (+13,5 cm)", "rimbalzo unico", "testa", "1-2", "tenuta f31", "DATI", None],
    ],
    "hitbox": [
        ["Piedi", 16, "atterraggio", "? (kit uasset)", "?", "?", "?", "? / yaw ?", "? (dati kit, illeggibili dal cloud)"],
    ],
    "corpo": [
        ["Hurtbox speciale (armor/intang.)", "nessuna ?"],
        ["Capsula", "? traccia capsula (consigliata) o clip dall'altezza del loop", None, None, None, None, None, "?", "decisione 5 aperta"],
        ["Dove (terra/aria)", "Aria→Terra", None, None, None, None, None, "OWNER"],
        ["Takeoff window", "n/a (mossa aerea)"],
        ["Atterraggio", "hold fino a terra → land clip", None, None, None, None, None, "OWNER", "track Hold Until Landed"],
        ["Landing lag f", "33 (= land 32 clip)", None, None, None, None, None, "DATI"],
        ["Max aria f", 125, None, None, None, None, None, "DATI", "2 s poi idle aria (dropkick-air-timeout-1)"],
        ["Ledge check", "n/a"],
    ],
    "cancel": [["Roll (da a terra)", "?", "?", "Landing", None, None, None, "OWNER", "combo extension: dettagli dall'owner"]],
    "giri": [
        ["r0", "2026-11-15", "6 buona", "base Antigravity", "sì", "buona", "good/dropkick-1 (da creare)", "OWNER", "atterraggio non arriva a terra; nessuna rialzata"],
        ["r1", "2026-11-15", "3 in gioco", "26733f24", "no", "peggio", None, "OWNER", "gambe aperte, rollio 35°: tratti 1-2 rotti"],
    ],
    "domande": [
        [1, "Rialzata veloce o animata (e come si combina con il roll)?", None, None, None, None, None, "owner", "?"],
        [2, "Caduta capsula 53 cm: traccia capsula o clip dall'altezza del loop?", None, None, None, None, None, "owner", "traccia capsula (~$10-15)"],
        [3, "FAF e dati hitbox (danno, angolo, KB) da fissare", None, None, None, None, None, "owner", None],
    ],
}
detail("Mossa_OV-FAIR", dk)

wb.move_sheet("Liste", offset=10)
wb.save(OUT)
print("ok", OUT)
