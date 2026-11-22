# Read-only: full docstrings of selection / timeline-selection calls and the scene setting handler.
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
ds = tab.domain_scene()
out = {}
out["select_doc"] = csc.domain.Selector.select.__doc__
out["set_full_selection_by_parts_doc"] = csc.layers.Selector.set_full_selection_by_parts.__doc__
out["selection_now"] = str(ds.get_layers_selector().selection())
ic = ds.get_layers_selector().selection()
out["indices_container_api"] = {n: (getattr(type(ic), n).__doc__ or "").split("\n")[0][:200] for n in dir(ic) if not n.startswith("_")}
sh = tab.get_setting_handler()
out["setting_handler_type"] = str(type(sh))
out["setting_handler_api"] = {n: (getattr(type(sh), n).__doc__ or "")[:300] for n in dir(sh) if not n.startswith("_")}
out["animation_boundary"] = str(tab.animation_boundary())
out["ab_api"] = [n for n in dir(tab.animation_boundary()) if not n.startswith("_")]
out["selected_now"] = str(ds.selector().selected())
sel = ds.selector().selected()
out["selected_api"] = [n for n in dir(sel) if not n.startswith("_")]
out["model_editor_api"] = None
def m(model, update, su):
    out["model_editor_api"] = [n for n in dir(model) if not n.startswith("_")]
    out["layers_editor_api"] = [n for n in dir(model.layers_editor()) if not n.startswith("_")]
    out["data_editor_api"] = [n for n in dir(model.data_editor()) if not n.startswith("_")]
    out["set_data_value_doc"] = type(model.data_editor()).set_data_value.__doc__[:1500]
ds.modify_update("probe (no change)", m)
print(json.dumps(out, default=str))
