"""E_run.py pre-PIE settings snapshot. Read-only assets; temporarily disables editor throttling."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out=Path(unreal.Paths.project_saved_dir())/'FeedbackE'
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
state={'throttle':perf.get_editor_property('bThrottleCPUWhenNotForeground'),
       'classes':[x.get_path_name() for x in fd.feedback_consumer_classes],
       'cvars':{n:unreal.SystemLibrary.get_console_variable_float_value(n) for n in
       ['t.MaxFPS','JJK.Feedback.HitStop','JJK.Feedback.Camera','JJK.Feedback.CameraStrength','JJK.Feedback.HUD','JJK.Feedback.Audio','r.ScreenPercentage']},
       'initial_health':fd.initial_health,'initial_cursed_energy':fd.initial_cursed_energy,'initial_energy':fd.initial_energy}
play=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorPlaySettings')
state['play_window']={n:play.get_editor_property(n) for n in ['NewWindowWidth','NewWindowHeight','CenterNewWindow']}
(out/'setup.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
classes=list(fd.feedback_consumer_classes)
if unreal.CombatAudioConsumer.static_class() not in classes:classes.append(unreal.CombatAudioConsumer.static_class())
fd.set_editor_property('feedback_consumer_classes',classes)
# This UE 5.8 Windows floating window adds 2 client pixels vertically; verify
# the resulting 1920x1080 game viewport in E_pie rather than trusting the request.
play.set_editor_property('NewWindowWidth',1920);play.set_editor_property('NewWindowHeight',1078);play.set_editor_property('CenterNewWindow',True)
print(state)
