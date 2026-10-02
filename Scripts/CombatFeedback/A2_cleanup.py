import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
state = json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackA2/setup.json').read_text(encoding='utf-8'))
for k, v in state['cvars'].items(): unreal.SystemLibrary.execute_console_command(None, k+' '+str(v))
unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground', state['throttle'])
play = unreal.load_object(None, '/Script/UnrealEd.Default__LevelEditorPlaySettings')
for k,v in state['play'].items(): play.set_editor_property(k,v)
unreal.load_object(None, '/Script/UnrealEd.Default__LevelEditorMiscSettings').set_editor_property('bAllowBackgroundAudio', state['allow_background_audio'])
unreal.CombatAudioDiagnostics.set_recording_background_volume(state['background_volume'])
dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
assert all(x.get_name().startswith('/Game/Training/DA_') for x in dirty), [x.get_name() for x in dirty]
if dirty: assert unreal.EditorLoadingAndSavingUtils.reload_packages(dirty, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
print('A2: temporary settings restored; no dirty package')
