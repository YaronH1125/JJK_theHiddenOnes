import unreal
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.SystemLibrary.quit_editor()
