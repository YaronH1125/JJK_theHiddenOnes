"""Temporary test settings only; production assets must already be wired on disk."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
assert fd.feedback_profile and fd.reaction_library and fd.feedback_profile.external_ranged_impact
for cls in [unreal.CombatPoseConsumer, unreal.CombatAudioConsumer, unreal.CombatRangedVisualConsumer]:
    assert list(fd.feedback_consumer_classes).count(cls.static_class()) == 1
perf = unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
play = unreal.load_object(None, '/Script/UnrealEd.Default__LevelEditorPlaySettings')
misc = unreal.load_object(None, '/Script/UnrealEd.Default__LevelEditorMiscSettings')
state = {'throttle': perf.get_editor_property('bThrottleCPUWhenNotForeground'),
         'play': {k: play.get_editor_property(k) for k in ['NewWindowWidth', 'NewWindowHeight', 'CenterNewWindow', 'SoloAudioInFirstPIEClient', 'EnableGameSound']},
         'allow_background_audio': misc.get_editor_property('bAllowBackgroundAudio'),
         'background_volume': unreal.CombatAudioDiagnostics.get_background_volume(),
         'cvars': {k: unreal.SystemLibrary.get_console_variable_float_value(k) for k in ['t.MaxFPS', 'r.ScreenPercentage', 'JJK.Feedback.HitStop', 'JJK.Feedback.Reaction', 'JJK.Feedback.Audio', 'JJK.Feedback.AudioGain', 'JJK.Feedback.RangedFX', 'JJK.Feedback.Camera', 'JJK.Feedback.CameraStrength', 'JJK.Feedback.HUD']}}
for key in ['JJK.Feedback.AudioLog','JJK.Feedback.Log']:
    state['cvars'][key]=unreal.SystemLibrary.get_console_variable_int_value(key)
(Path(unreal.Paths.project_saved_dir())/'FeedbackA2/setup.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
play.set_editor_property('NewWindowWidth', 1280); play.set_editor_property('NewWindowHeight', 718)
play.set_editor_property('CenterNewWindow', True); play.set_editor_property('SoloAudioInFirstPIEClient', True); play.set_editor_property('EnableGameSound', True)
misc.set_editor_property('bAllowBackgroundAudio', True)
unreal.CombatAudioDiagnostics.set_recording_background_volume(1.)
print('A2: saved production binding verified; temporary audio/capture settings ready')
