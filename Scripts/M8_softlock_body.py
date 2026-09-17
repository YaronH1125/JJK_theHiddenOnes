
import unreal,json,math
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
 p1.character_movement.stop_movement_immediately()
 p1.set_actor_location(unreal.Vector(-160,0,100),False,True)
 p2.set_actor_location(unreal.Vector(160,0,100),False,True)
 p1.set_actor_rotation(unreal.Rotator(0,180,0),False)
 yield from wait(.2)
 fwd=math.atan2(p1.get_actor_forward_vector().y,p1.get_actor_forward_vector().x)
 p1.get_combat_input().submit_light_attack()
 yield from wait(.6)
 fwd2=math.atan2(p1.get_actor_forward_vector().y,p1.get_actor_forward_vector().x)
 tgt=math.atan2(p2.get_actor_location().y-p1.get_actor_location().y,p2.get_actor_location().x-p1.get_actor_location().x)
 d2t=math.degrees(abs(math.atan2(math.sin(tgt-fwd2),math.cos(tgt-fwd2))))
 turned=math.degrees(abs(math.atan2(math.sin(fwd2-fwd),math.cos(fwd2-fwd))))
 camYaw=pc.get_control_rotation().yaw
 dCam=math.degrees(abs(math.atan2(math.sin(math.radians(tgt-camYaw)),math.cos(math.radians(tgt-camYaw)))))
 unreal.log('SOFTLOCK turned=%.1f face_err=%.1f cam_err=%.1f'%(turned,d2t,dCam))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('SLERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
