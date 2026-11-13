"""GSTN Studio: the owner's window on the pipeline, without Cascadeur open.  python -m animpipe.app

Moves with their `status`; the newest round's composite grid next to its verdict; Ok (records the owner's words with
owner-ok) and Approva; the open variant against the original with the changed frames and their why, Applica / Scarta;
snapshots with date and reason, Ripristina; Annulla ultimo import. Every button runs one `animpipe` verb as a separate
process and shows its json line: the app has no logic of its own, so there is no second path through the pipeline.

Tkinter (stdlib) instead of PySide6, as planned in the proposal: nothing to install, and Tk 8.6 reads PNG directly.
"""
import json, os, subprocess, sys, threading
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk

from animpipe import config as K, verbs as V

T = {"it": {"title": "GSTN Studio", "moves": "Mosse", "refresh": "Aggiorna", "ok": "Ok", "approve": "Approva",
            "apply": "Applica variante", "discard": "Scarta variante", "undo": "Annulla ultimo import",
            "snaps": "Snapshot", "restore": "Ripristina", "orig": "Originale", "var": "Variante", "noimg": "nessuna immagine",
            "quote": "Le tue parole per questo ok:", "sure_approve": "Bloccare le pose di {m}?",
            "sure_apply": "Applicare la variante a {m}? L'originale prende le pose della variante.",
            "sure_restore": "Ripristinare {m} allo snapshot {s}?", "sure_undo": "Rimettere {m} com'era prima dell'ultimo import?",
            "verdict": "Verdetto", "none": "nessun verdetto", "next": "Prossimo passo"},
     "en": {"title": "GSTN Studio", "moves": "Moves", "refresh": "Refresh", "ok": "Ok", "approve": "Approve",
            "apply": "Apply variant", "discard": "Discard variant", "undo": "Undo last import",
            "snaps": "Snapshots", "restore": "Restore", "orig": "Original", "var": "Variant", "noimg": "no image",
            "quote": "Your words for this ok:", "sure_approve": "Lock the poses of {m}?",
            "sure_apply": "Apply the variant to {m}? The original takes the variant's poses.",
            "sure_restore": "Restore {m} to snapshot {s}?", "sure_undo": "Put {m} back as it was before the last import?",
            "verdict": "Verdict", "none": "no verdict", "next": "Next step"}}
L = T.get(K.CFG.get("owner_lang", "it"), T["en"])


def animpipe(*args):
    """Runs one verb, returns its json line as a dict."""
    p = subprocess.run([sys.executable, "-m", "animpipe", *args], cwd=K.TOOLS, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", env=dict(os.environ, PYTHONIOENCODING="utf-8"))
    lines = [l for l in p.stdout.splitlines() if l.strip().startswith("{")]
    try:
        return json.loads(lines[-1])
    except (IndexError, ValueError):
        return {"ok": False, "exit": p.returncode, "result": {"error": (p.stderr or p.stdout)[-300:]}}


def newest_round(move):
    d = os.path.join(K.VISUAL, move)
    rs = sorted((int(n[1:]) for n in os.listdir(d) if n[1:].isdigit()), reverse=True) if os.path.isdir(d) else []
    return (rs[0], os.path.join(d, f"r{rs[0]}")) if rs else (None, None)


def verdict_text(folder):
    p = os.path.join(folder or "", "verdict.json")
    if not folder or not os.path.exists(p):
        return L["none"]
    v = json.load(open(p, encoding="utf-8"))
    out = [f"esito: {v.get('esito')}"]
    for q in v.get("problemi", []):
        out.append(f"[{q.get('gravita')}] {q.get('regola')} f{q.get('frame')} {','.join(q.get('parti', []))}: {q.get('prova', '')}")
    bad = [f"{s['da']}-{s['a']} {s['stato']}" for s in v.get("segmenti", []) if s.get("stato") != "ok"]
    out.append("segmenti: " + (", ".join(bad) if bad else "tutti ok"))
    ok = os.path.join(folder, "owner_ok.json")
    if os.path.exists(ok):
        out.append(f"ok owner: \"{json.load(open(ok, encoding='utf-8')).get('quote')}\"")
    return "\n".join(out)


class Studio(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(L["title"]); self.geometry("1400x860"); self.minsize(900, 600)
        self.move, self.imgs = None, {}
        left = ttk.Frame(self, padding=6); left.pack(side="left", fill="y")
        ttk.Label(left, text=L["moves"]).pack(anchor="w")
        self.list = tk.Listbox(left, width=26, exportselection=False); self.list.pack(fill="y", expand=True)
        self.list.bind("<<ListboxSelect>>", lambda e: self.select())
        ttk.Button(left, text=L["refresh"], command=self.load_moves).pack(fill="x", pady=(6, 0))
        ttk.Label(left, text=L["snaps"]).pack(anchor="w", pady=(10, 0))
        self.snaps = tk.Listbox(left, width=26, height=10, exportselection=False); self.snaps.pack(fill="x")
        ttk.Button(left, text=L["restore"], command=self.restore).pack(fill="x", pady=(4, 0))
        ttk.Button(left, text=L["undo"], command=self.undo_import).pack(fill="x", pady=(4, 0))

        right = ttk.Frame(self, padding=6); right.pack(side="left", fill="both", expand=True)
        self.status = ttk.Label(right, text="", font=("Segoe UI", 11, "bold"), wraplength=1000); self.status.pack(anchor="w")
        bar = ttk.Frame(right); bar.pack(fill="x", pady=4)
        self.b_ok = ttk.Button(bar, text=L["ok"], command=self.owner_ok); self.b_ok.pack(side="left")
        self.b_ap = ttk.Button(bar, text=L["approve"], command=self.approve); self.b_ap.pack(side="left", padx=4)
        self.b_apply = ttk.Button(bar, text=L["apply"], command=self.apply_variant); self.b_apply.pack(side="left", padx=(20, 4))
        self.b_disc = ttk.Button(bar, text=L["discard"], command=self.discard_variant); self.b_disc.pack(side="left")
        body = ttk.Frame(right); body.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(body, background="#202020", highlightthickness=0); self.canvas.pack(side="left", fill="both", expand=True)
        side = ttk.Frame(body, width=360); side.pack(side="left", fill="y", padx=(6, 0))
        ttk.Label(side, text=L["verdict"]).pack(anchor="w")
        self.text = tk.Text(side, width=48, wrap="word", font=("Consolas", 9)); self.text.pack(fill="both", expand=True)
        self.out = ttk.Label(self, text="", relief="sunken", anchor="w"); self.out.pack(side="bottom", fill="x")
        self.canvas.bind("<Configure>", lambda e: self.after_idle(self.draw))
        self.load_moves()

    # --- helpers ---
    def run(self, *args, then=None):
        self.out.config(text="... animpipe " + " ".join(args))
        def work():
            r = animpipe(*args)
            self.after(0, lambda: (self.out.config(text=json.dumps(r, ensure_ascii=False)[:400]), then and then(r), self.select()))
        threading.Thread(target=work, daemon=True).start()

    def load_moves(self):
        self.list.delete(0, "end")
        for m in V.moves():
            self.list.insert("end", m)

    def select(self):
        sel = self.list.curselection()
        if not sel:
            return
        self.move = self.list.get(sel[0])
        try:
            st = V.state(self.move)
        except Exception as e:                        # a card that does not load: show why, keep the window alive
            st = {"next": "?", "why": str(e)[:200], "owner": False}
        self.st = st
        self.status.config(text=f"{self.move}  |  {L['next']}: {st['next']}  ({st['why']})")
        var = V.open_variant(self.move)
        for b in (self.b_apply, self.b_disc):
            b.state(["!disabled"] if var else ["disabled"])
        rnd, folder = newest_round(self.move)
        self.views = [(L["orig"] + (f" r{rnd}" if rnd else ""), os.path.join(folder, "grid.png") if folder else None)]
        txt = verdict_text(folder)
        if var:
            vname = os.path.basename(var)[:-5]
            vr, vf = newest_round(vname)
            self.views.append((f"{L['var']} {vname}" + (f" r{vr}" if vr else ""), os.path.join(vf, "grid.png") if vf else None))
            c, v = V.read(V.card_path(self.move)), V.read(var)
            vq = {q["frame"]: q for q in v["poses"]}
            ch = sorted({int(f) for f in set(c["author"]["poses"]) | set(v["author"]["poses"])
                         if c["author"]["poses"].get(f) != v["author"]["poses"].get(f)})
            txt += f"\n\n{L['var']} {vname}: {v['variant']['why']}\n" + "\n".join(
                f"  f{f}: {(vq.get(f) or {}).get('why', '-')}" for f in ch) + "\n\n" + verdict_text(vf)
        self.text.delete("1.0", "end"); self.text.insert("1.0", txt)
        self.snaps.delete(0, "end")
        self.snap_ids = []
        for m in reversed(V.S.listing(self.move)):
            self.snaps.insert("end", f"{m['stamp'][:8]} {m['stamp'][9:15]}  {m.get('reason', '')}"); self.snap_ids.append(m)
        self.draw()

    def draw(self):
        self.canvas.delete("all")
        if not self.move:
            return
        W, H = max(self.canvas.winfo_width(), 100), max(self.canvas.winfo_height(), 100)
        n = len(self.views); w = W // n
        for i, (label, path) in enumerate(self.views):
            self.canvas.create_text(i * w + 8, 8, anchor="nw", text=label, fill="#dddddd", font=("Segoe UI", 10, "bold"))
            if not path or not os.path.exists(path):
                self.canvas.create_text(i * w + w // 2, H // 2, text=L["noimg"], fill="#888888"); continue
            key = (path, os.path.getmtime(path), w, H)
            if key not in self.imgs:
                img = tk.PhotoImage(file=path)
                f = max(1, -(-img.width() // (w - 8)), -(-img.height() // (H - 30)))
                self.imgs[key] = img.subsample(f) if f > 1 else img
            self.canvas.create_image(i * w + 4, 28, anchor="nw", image=self.imgs[key])

    # --- buttons: one verb each ---
    def owner_ok(self):
        if self.move:
            q = simpledialog.askstring(L["title"], L["quote"], initialvalue="ok", parent=self)
            if q:
                self.run("owner-ok", self.move, f"--quote={q}", "--by=owner (GSTN Studio)")

    def approve(self):
        if self.move and messagebox.askyesno(L["title"], L["sure_approve"].format(m=self.move)):
            self.run("approve", self.move)

    def apply_variant(self):
        if self.move and messagebox.askyesno(L["title"], L["sure_apply"].format(m=self.move)):
            self.run("variant", "apply", self.move, "--quote=applica (GSTN Studio)")

    def discard_variant(self):
        if self.move:
            self.run("variant", "discard", self.move)

    def restore(self):
        sel = self.snaps.curselection()
        if self.move and sel:
            s = self.snap_ids[sel[0]]["stamp"]
            if messagebox.askyesno(L["title"], L["sure_restore"].format(m=self.move, s=s)):
                self.run("restore", self.move, s)

    def undo_import(self):
        if self.move and messagebox.askyesno(L["title"], L["sure_undo"].format(m=self.move)):
            self.run("revert", self.move)


def main():
    Studio().mainloop()


if __name__ == "__main__":
    main()
