import unreal
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert all(p.get_path_name()=='/Game/Training/DA_Fighter_Ishigori' for p in dirty)
# Only discard this test's restored-but-dirty in-memory definition; do not save any assets.
if dirty:
    result=unreal.EditorLoadingAndSavingUtils.reload_packages(dirty,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    assert result[0],str(result)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
