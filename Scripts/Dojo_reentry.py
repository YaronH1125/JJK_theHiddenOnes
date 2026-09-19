import unreal,json
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
fighters=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.FighterCharacter)
extent=gm.get_editor_property('arena_walkable_half_extent')
assert 'L_DojoArena' in w.get_path_name() and len(fighters)==2
assert extent.x==804 and extent.y==652 and not gm.get_editor_property('allow_conditional_throw')
assert 130<p1.get_actor_location().z<145 and 130<p2.get_actor_location().z<145
unreal.SystemLibrary.execute_console_command(w,'HighResShot 1')
print(json.dumps({'map':w.get_path_name(),'fighters':len(fighters),'p1':str(p1.get_actor_location()),'p2':str(p2.get_actor_location()),'extent':str(extent)},ensure_ascii=False))
