"""Release the idle Editor to F after tests; never discards dirty packages."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
data={'PIE':False,'dirty_content':[],'dirty_maps':[],'requested_editor_exit':True,
      'cvars':{k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in ['JJK.Feedback.HitStop','JJK.Feedback.Reaction','JJK.Feedback.RangedFX','JJK.Feedback.Audio','JJK.Feedback.AudioGain','JJK.Feedback.Camera','JJK.Feedback.CameraStrength','JJK.Feedback.HUD']}}
(Path(unreal.Paths.project_saved_dir())/'FeedbackA2/editor-handoff.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
