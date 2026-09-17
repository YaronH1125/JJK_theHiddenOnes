
import unreal, json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
gm.reset_training()
LINES=[]
for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor):
    n=a.get_name()
    if 'Static' in n:
        LINES.append('%s loc=%s scale=%s'%(n,a.get_actor_location(),a.get_actor_scale3d()))
unreal.log('PROBE:'+json.dumps(LINES))
