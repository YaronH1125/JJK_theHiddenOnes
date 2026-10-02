"""After E's owned PIE ends, restore settings and reload only its temporary fighter fixture."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out=Path(unreal.Paths.project_saved_dir())/'FeedbackE'
state=json.loads((out/'setup.json').read_text(encoding='utf-8'))
for n,v in state['cvars'].items():unreal.SystemLibrary.execute_console_command(None,n+' '+str(v))
unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground',state['throttle'])
play=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorPlaySettings')
for n,v in state['play_window'].items():play.set_editor_property(n,v)
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
for n in ['initial_health','initial_cursed_energy','initial_energy']:fd.set_editor_property(n,state[n])
dirty=list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
assert all(x.get_name()=='/Game/Training/DA_Fighter_Ishigori' for x in dirty),[x.get_name() for x in dirty]
if dirty:assert unreal.EditorLoadingAndSavingUtils.reload_packages(dirty,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert [x.get_path_name() for x in unreal.load_asset('/Game/Training/DA_Fighter_Ishigori').feedback_consumer_classes]==state['classes']
(out/'cleanup.json').write_text(json.dumps({'restored':True,'dirty':[],'settings':state},indent=2),encoding='utf-8')
print('E restored all temporary settings; no shared asset saved')
