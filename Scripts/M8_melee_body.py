
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.reset_training();yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 fd=p1.get_definition()
 unreal.log('PROBE autoface=%s'%fd.get_editor_property('melee_auto_face'))
 p1.character_movement.stop_movement_immediately()
 p1.set_actor_location(unreal.Vector(-100,0,100),False,True)
 p2.set_actor_location(unreal.Vector(100,0,100),False,True)
 p1.set_actor_rotation(unreal.Rotator(0,0,0),False)
 yield from wait(.3)
 h0=p2.get_fighter_attribute_set().health.current_value
 p1.get_combat_input().submit_light_attack()
 yield from wait(1.2)
 h1=p2.get_fighter_attribute_set().health.current_value
 unreal.log('PROBE melee hit delta=%s'%(h0-h1))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('PROBEERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
