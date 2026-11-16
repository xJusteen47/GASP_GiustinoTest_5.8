import csc, json
app = csc.app.get_application()
dsm = app.get_data_source_manager()
out = {"dsm": [n for n in dir(dsm) if not n.startswith("_")]}
for n in out["dsm"]:
    out["doc_" + n] = (getattr(type(dsm), n).__doc__ or "")[:300]
print(json.dumps(out))
