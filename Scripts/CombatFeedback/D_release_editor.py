"""Release D's idle Editor after cleanup. python Scripts/ue_python.py Scripts/CombatFeedback/D_release_editor.py"""
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
