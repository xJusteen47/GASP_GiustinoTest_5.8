# Cascadeur: make the Live Link test copy current and toggle "Live Link (Start UE connection)".
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_livelink_test.casc"][0]
sm.set_current_scene(tab)
app.get_action_manager().call_action("View.Live Link (Start UE connection)")
print(json.dumps({"current": sm.current_scene().name()}))
