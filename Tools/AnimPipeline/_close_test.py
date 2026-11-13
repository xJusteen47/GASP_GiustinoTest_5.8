# Turn AutoPhysics off on the physics test tab, then close it without saving (frees memory).
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test2.casc"][0]
app.get_tools_manager().get_tool("AutoPhysicsTool").editor(tab).turn_off()
ll = [s for s in sm.scenes() if s.name() == "jab_livelink_test.casc"][0]
sm.set_current_scene(ll)
app.get_data_source_manager().close_scene(tab)
print(json.dumps({"tabs": [s.name() for s in sm.scenes()], "current": sm.current_scene().name()}))
