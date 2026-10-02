"""After B_run stops its PIE, discard only B's unsaved test property edits.

python Scripts/ue_python.py Scripts/CombatFeedback/B_cleanup.py
Restores captured CVars/performance option and reloads the two test packages
from unchanged disk files. Refuses unknown dirty content. Never saves anything.
"""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
root=Path(unreal.Paths.project_saved_dir())/'FeedbackB'
latest=json.loads((root/'latest-run.json').read_text())
settings=Path(latest['path'])/'restore-settings.json'
if settings.exists():
    restore=json.loads(settings.read_text())
    unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground',restore['throttle'])
    for n,value in restore['cvars'].items():unreal.SystemLibrary.execute_console_command(None,n+' '+str(value))
allowed={'/Game/Training/DA_Fighter_Ishigori','/Game/Training/DA_M3_HeavyPunch'}
dirty=list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
assert all(p.get_name() in allowed for p in dirty),[p.get_name() for p in dirty]
if dirty:
    result=unreal.EditorLoadingAndSavingUtils.reload_packages(dirty,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    assert result[0],result
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SkeletalMeshActor)
for a in actors:
    if 'FeedbackB_TransientPreview' in [str(t) for t in a.tags]:unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(a)
state=dict(pie=False,reloaded=[p.get_name() for p in dirty],dirty_content_after=[p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
           library=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori').reaction_library.get_path_name() if unreal.load_asset('/Game/Training/DA_Fighter_Ishigori').reaction_library else None,
           hit_stop=unreal.SystemLibrary.get_console_variable_int_value('JJK.Feedback.HitStop'))
assert not state['dirty_content_after'] and state['library']==(restore.get('library') if settings.exists() else None) and state['hit_stop']==(restore['cvars']['JJK.Feedback.HitStop'] if settings.exists() else 0),state
(root/'editor-handoff.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
print(json.dumps(state))
