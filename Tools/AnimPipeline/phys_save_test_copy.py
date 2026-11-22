# Save the throwaway physics test tab under its own copy name (no other change).
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
p = str(tab.get_path_name())
assert p.endswith("/Saved/AnimPipeline/jab_phys_test.casc"), p
tab.save(p)
print(json.dumps({"saved": p}))
