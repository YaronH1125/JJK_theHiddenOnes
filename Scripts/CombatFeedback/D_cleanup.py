"""After StopPIE restore D test settings and reload only the D test-dirty fighter DA."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out=Path(unreal.Paths.project_saved_dir())/'FeedbackD'
state=json.loads((out/'setup.json').read_text(encoding='utf-8'))
for n,v in state['cvars'].items(): unreal.SystemLibrary.execute_console_command(None,n+' '+str(v))
unreal.SystemLibrary.execute_console_command(None,'JJK.Feedback.AudioGain '+str(state['audio_gain']))
unreal.SystemLibrary.execute_console_command(None,'r.ScreenPercentage '+str(state['screen_percentage']))
unreal.CombatAudioDiagnostics.set_recording_background_volume(state['background_volume'])
play=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorPlaySettings')
play.set_editor_property('SoloAudioInFirstPIEClient',state['solo_audio'])
play.set_editor_property('EnableGameSound',state['game_sound'])
unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorMiscSettings').set_editor_property('bAllowBackgroundAudio',state['allow_background_audio'])
unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground',state['throttle'])
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
fd.set_editor_property('initial_energy',state['initial_energy'])
fd.set_editor_property('initial_cursed_energy',state['initial_cursed_energy'])
dirty=list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
assert all(x.get_name()=='/Game/Training/DA_Fighter_Ishigori' for x in dirty),[x.get_name() for x in dirty]
if dirty: assert unreal.EditorLoadingAndSavingUtils.reload_packages(dirty,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
assert [x.get_path_name() for x in fd.feedback_consumer_classes]==state['classes']
assert fd.initial_energy==state['initial_energy']
assert fd.initial_cursed_energy==state['initial_cursed_energy']
restored={'restored':True,'classes':state['classes'],'initial_energy':fd.initial_energy,
          'initial_cursed_energy':fd.initial_cursed_energy,'screen_percentage':unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'),
          'audio_gain':unreal.SystemLibrary.get_console_variable_float_value('JJK.Feedback.AudioGain'),
          'background_volume':unreal.CombatAudioDiagnostics.get_background_volume(),
          'allow_background_audio':unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorMiscSettings').get_editor_property('bAllowBackgroundAudio'),
          'solo_audio':play.get_editor_property('SoloAudioInFirstPIEClient'),'game_sound':play.get_editor_property('EnableGameSound'),
          'dirty':[x.get_name() for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
assert restored['background_volume']==state['background_volume']
assert restored['allow_background_audio']==state['allow_background_audio']
assert restored['solo_audio']==state['solo_audio']
assert restored['game_sound']==state['game_sound']
(out/'cleanup.json').write_text(json.dumps(restored,indent=2),encoding='utf-8')
print('D settings and disk fighter binding restored')
