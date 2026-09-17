import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
OUT=Path(unreal.Paths.project_saved_dir())/'M7_diag_charge.json'
LINES=[]
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
 yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 LINES.append('stance ranged=%s'%tag(p1,'Stance.Ranged'))
 p1.get_targeting().lock_best_target()
 p1.character_movement.stop_movement_immediately()
 p1.set_actor_location(unreal.Vector(-500,0,100),False,True)
 p2.set_actor_location(unreal.Vector(500,0,100),False,True)
 yield from wait(.3)
 c0=p1.get_fighter_attribute_set().cursed_energy.current_value
 t_press=unreal.GameplayStatics.get_time_seconds(world)
 p1.get_combat_input().notify_kick_pressed()
 yield from wait(1.35)
 p1.get_combat_input().notify_kick_released()
 t_release=unreal.GameplayStatics.get_time_seconds(world)
 yield from wait(1.0)
 c1=p1.get_fighter_attribute_set().cursed_energy.current_value
 h1=p2.get_fighter_attribute_set().health.current_value
 LINES.append('hold=%.3f cost=%.3f hp_drop_seen=%.1f'%(t_release-t_press,c0-c1,h1<999.9))
gen=suite()
def tick(dt):
 try: next(gen)
 except StopIteration: unreal.unregister_slate_post_tick_callback(handle);OUT.write_text(json.dumps(LINES,ensure_ascii=False),encoding='utf-8')
handle=unreal.register_slate_post_tick_callback(tick)
