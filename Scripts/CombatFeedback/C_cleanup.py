"""Restore C's isolated test settings and reload only the test-dirty fighter DA.

Called by C_run.py after StopPIE. Never saves a shared asset.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC'
state = json.loads((out / 'setup.json').read_text(encoding='utf-8'))
for name, value in state['cvars'].items():
    unreal.SystemLibrary.execute_console_command(None, name + ' ' + str(value))
unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property(
    'bThrottleCPUWhenNotForeground', state['throttle'])
dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
assert all(x.get_name() == '/Game/Training/DA_Fighter_Ishigori' for x in dirty), [x.get_name() for x in dirty]
if dirty:
    reloaded = unreal.EditorLoadingAndSavingUtils.reload_packages(
        dirty, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    assert reloaded[0], reloaded
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
now = {'classes': [x.get_path_name() for x in fd.feedback_consumer_classes],
       'profile': fd.feedback_profile.get_path_name() if fd.feedback_profile else None,
       'dirty': [x.get_name() for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
assert now['classes'] == state['old_classes'] and now['profile'] == state['old_profile'] and not now['dirty'], now
(out / 'editor-handoff.json').write_text(json.dumps(now, indent=2), encoding='utf-8')
print(json.dumps(now))
