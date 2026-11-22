"""ultimateframedata.com reference fetcher: one page request per character (cached for a week in refs/ufd/pages/), the
frame data of the matching moves appended to refs/ufd/ufd_framedata.csv, their hitbox GIFs downloaded next to it (owner's
standing OK for reference GIFs) and measured with ufd_measure.py (extension, impact spacing, hold, recoil).
Polite by design: no crawling, one request per page per week, plus one per GIF not already on disk.

Usage: python ufd_fetch.py <character slug> ["move name filter"] [--no-gif] [--strength light|strong]
  e.g. python ufd_fetch.py terry "forward tilt"      python ufd_fetch.py little_mac "smash"
Character slugs are the page names on the site (mario, little_mac, ryu, terry, captain_falcon, ...)."""
import os, re, sys, csv, time, json, html, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
UFD = os.path.join(HERE, "refs", "ufd")
PAGES = os.path.join(UFD, "pages")
SITE = "https://ultimateframedata.com/"
UA = {"User-Agent": "Mozilla/5.0 (GSTN anim reference fetcher; a few requests, cached)"}
FIELDS = ["movename", "startup", "totalframes", "landinglag", "notes", "basedamage", "shieldlag", "shieldstun",
          "whichhitbox", "advantage", "activeframes", "endlag"]

def get(url, path, max_age_s=7 * 24 * 3600):
    if os.path.exists(path) and time.time() - os.path.getmtime(path) < max_age_s:
        return open(path, "rb").read()
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=30) as r:
        data = r.read()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(data)
    return data

def parse(page):
    out = []
    for block in re.findall(r'<div class="movecontainer">(.*?)</div>\s*</div>', page, flags=re.S):
        row = {}
        for f in FIELDS:
            m = re.search(rf'<div class="{f}">(.*?)</div>', block, flags=re.S)
            row[f] = re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", "", m.group(1)))).strip() if m else ""
        g = re.search(r'data-featherlight="(hitboxes/[^"]+\.gif)"', block)
        row["hitbox_gif"] = SITE + g.group(1) if g else ""
        if row["movename"]:
            out.append(row)
    return out

def first_last_active(s):
    nums = [int(x) for x in re.findall(r"\d+", s.split("/")[0])]
    return (nums[0], nums[-1]) if nums else (None, None)

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    slug = args[0].lower()
    flt = args[1].lower() if len(args) > 1 else ""
    want_gif = "--no-gif" not in sys.argv
    strength = sys.argv[sys.argv.index("--strength") + 1] if "--strength" in sys.argv else None
    page = get(SITE + slug, os.path.join(PAGES, slug + ".html")).decode("utf-8", errors="ignore")
    moves = [m for m in parse(page) if flt in m["movename"].lower()]
    if not moves:
        sys.exit(f"no move matching '{flt}' on {slug}; moves: {[m['movename'] for m in parse(page)][:60]}")
    csv_path = os.path.join(UFD, "ufd_framedata.csv")
    have = set()
    if os.path.exists(csv_path):
        with open(csv_path, newline="", encoding="utf-8") as fh:
            have = {(r["character"].lower(), r["move"].lower()) for r in csv.DictReader(fh)}
    rows = []
    for m in moves:
        a0, a1 = first_last_active(m["activeframes"])
        tot = re.findall(r"\d+", m["totalframes"])
        row = {"character": slug.replace("_", " ").title(), "move": m["movename"], "startup": m["startup"], "active": m["activeframes"],
               "total_frames": m["totalframes"], "faf_ufd_convention": int(tot[0]) + 1 if tot else "", "on_shield": m["advantage"],
               "base_damage": m["basedamage"], "notes": m["notes"], "hitbox_gif": m["hitbox_gif"], "source": SITE + slug,
               "retrieved": time.strftime("%Y-%m-%d")}
        rows.append((row, a0, a1))
        if (row["character"].lower(), row["move"].lower()) not in have:
            new = not os.path.exists(csv_path)
            with open(csv_path, "a", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=list(row)); w.writeheader() if new else None; w.writerow(row)
    results = []
    for row, a0, a1 in rows:
        res = {"move": f"{row['character']} {row['move']}", "startup": row["startup"], "active": row["active"], "total": row["total_frames"]}
        if want_gif and row["hitbox_gif"] and a0:
            gif = os.path.basename(row["hitbox_gif"])
            get(row["hitbox_gif"], os.path.join(UFD, gif), max_age_s=10 ** 9)
            import ufd_measure
            st = strength or ("strong" if any(k in row["move"].lower() for k in ("smash", "tilt", "special")) else "light")
            try:
                res["measured"] = {k: v for k, v in ufd_measure.measure(res["move"], gif, a0, a1, st).items() if k != "extension_curve_pct"}
            except Exception as e:
                res["measured"] = f"measure failed: {e}"
        results.append(res)
    print(json.dumps(results, indent=1))
