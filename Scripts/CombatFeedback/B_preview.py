"""Place transient animation preview actors, or clean them up. Never save map.

Write Saved/FeedbackB/preview_request.json with items [{asset, time, y, yaw}],
then run: python Scripts/ue_python.py Scripts/CombatFeedback/B_preview.py
Use {"items": []} to remove previews. Root/mesh scale matches live BP_Fighter.
Capture with Scripts/ue_capture.py and an explicit camera transform.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SkeletalMeshActor):
    if 'FeedbackB_TransientPreview' in [str(t) for t in a.tags]:
        actors.destroy_actor(a)
request = json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackB/preview_request.json').read_text(encoding='utf-8'))
for item in request['items']:
    a = actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(item.get('x',0),item.get('y',0),0),unreal.Rotator(pitch=0,yaw=item.get('yaw',-90),roll=0),transient=True)
    a.set_editor_property('tags',[unreal.Name('FeedbackB_TransientPreview')])
    mesh = a.skeletal_mesh_component
    mesh.set_skeletal_mesh_asset(unreal.load_asset('/Game/Characters/Ishigori/Repaired/SK_Ishigori_Repaired'))
    mesh.set_world_scale3d(unreal.Vector(1.9369,1.9369,1.9369))
    mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    mesh.set_update_animation_in_editor(True)
    mesh.override_animation_data(unreal.load_asset(item['asset']),False,False,item['time'],1.)
    mesh.set_position(item['time'],False)
print('Preview actors:',len(request['items']))
