"""Editor-side: read (and optionally set) EditorPerformanceSettings.throttle_cpu_when_not_foreground. Not exposed to Python in
UE 5.8 (the property lookup fails), kept as the record of that test."""
import unreal, json
def main(set_value=None):
    cls = unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')
    s = unreal.get_default_object(cls)
    before = s.get_editor_property("throttle_cpu_when_not_foreground")
    if set_value is not None:
        s.set_editor_property("throttle_cpu_when_not_foreground", bool(set_value))
        try:
            s.save_config()
        except Exception as e:
            pass
    return json.dumps({"before": before, "after": s.get_editor_property("throttle_cpu_when_not_foreground")})
