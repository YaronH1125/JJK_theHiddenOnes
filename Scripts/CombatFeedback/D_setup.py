"""Temporary D binding for isolated PIE. D_run.py calls this; never saves shared DA."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
classes=list(fd.feedback_consumer_classes)
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
names=['t.MaxFPS','JJK.Feedback.HitStop','JJK.Feedback.Audio','JJK.Feedback.AudioLog','JJK.Feedback.Log']
state={'classes':[x.get_path_name() for x in classes], 'throttle':perf.get_editor_property('bThrottleCPUWhenNotForeground'),
       'cvars':{n:unreal.SystemLibrary.get_console_variable_int_value(n) for n in names},
       'audio_gain':unreal.SystemLibrary.get_console_variable_float_value('JJK.Feedback.AudioGain'),
       'initial_energy':fd.initial_energy, 'initial_cursed_energy':fd.initial_cursed_energy}
play=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorPlaySettings')
state['solo_audio']=play.get_editor_property('SoloAudioInFirstPIEClient')
state['game_sound']=play.get_editor_property('EnableGameSound')
misc=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorMiscSettings')
state['allow_background_audio']=misc.get_editor_property('bAllowBackgroundAudio')
state['background_volume']=unreal.CombatAudioDiagnostics.get_background_volume()
state['screen_percentage']=unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage')
out=Path(unreal.Paths.project_saved_dir())/'FeedbackD'
(out/'setup.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
consumer=unreal.CombatAudioConsumer.static_class()
if consumer not in classes: classes.append(consumer)
fd.set_editor_property('feedback_consumer_classes',classes)
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
play.set_editor_property('SoloAudioInFirstPIEClient',True)
play.set_editor_property('EnableGameSound',True)
misc.set_editor_property('bAllowBackgroundAudio',True)
unreal.CombatAudioDiagnostics.set_recording_background_volume(1.)
unreal.SystemLibrary.execute_console_command(None,'r.ScreenPercentage 30')
assert unreal.get_default_object(unreal.CombatAudioConsumer).audio_profile
print(json.dumps(state))
