"""Read live audio/editor test settings without mutation. python Scripts/ue_python.py Scripts/CombatFeedback/D_audio_settings.py"""
import json
from pathlib import Path
import unreal
s=unreal.load_object(None,'/Script/UnrealEd.Default__LevelEditorPlaySettings')
r={}
for name in ['EnableGameSound','SoloAudioInFirstPIEClient','bUseNonRealtimeAudioDevice']:
    try:r[name]=s.get_editor_property(name)
    except Exception as e:r[name]=str(e)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if w:
    gm=unreal.GameplayStatics.get_player_controller(w,0).get_training_game_mode()
    for label,actor in [('p',gm.get_player_fighter()),('q',gm.get_opponent_fighter())]:
        r[label]=[]
        for c in actor.get_components_by_class(unreal.AudioComponent):
            r[label].append({'name':c.get_name(),'sound':str(c.sound),'playing':c.is_playing(),'volume':c.volume_multiplier,'location':str(c.get_world_location())})
out=Path(unreal.Paths.project_saved_dir())/'FeedbackD'
(out/'audio-settings.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
print(json.dumps(r,indent=2))
