import csc
sm = csc.app.get_application().get_scene_manager()
sm.set_current_scene([s for s in sm.scenes() if s.name() == 'jab_v1.casc'][0])
print(sm.current_scene().name(), [s.name() for s in sm.scenes()])
