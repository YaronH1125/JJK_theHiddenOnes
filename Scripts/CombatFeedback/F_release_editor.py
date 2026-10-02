"""End exclusive F editor session after restoration. Via Scripts/ue_python.py."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
report={'PIE':False,'dirty_content':[],'dirty_maps':[],
        'defaults':{k:fd.get_editor_property(k) for k in ['initial_health','initial_cursed_energy','initial_energy']},
        'cvars':{k:unreal.SystemLibrary.get_console_variable_float_value(k) for k in ['JJK.Feedback.HitStop','JJK.Feedback.Reaction','JJK.Feedback.Audio','JJK.Feedback.AudioGain','JJK.Feedback.RangedFX','JJK.Feedback.Camera','JJK.Feedback.CameraStrength','JJK.Feedback.HUD']},
        'consumers':[x.get_path_name() for x in fd.feedback_consumer_classes],
        'requested_exit':True}
assert report['defaults']=={'initial_health':1000.,'initial_cursed_energy':100.,'initial_energy':0.}
assert report['cvars']['JJK.Feedback.HitStop']==0
(Path(unreal.Paths.project_saved_dir())/'CombatFeedback/F/editor-handoff.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
