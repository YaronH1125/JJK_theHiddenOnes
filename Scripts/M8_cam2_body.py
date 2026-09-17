
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
 boom=p1.get_component_by_class(unreal.SpringArmComponent)
 cam=p1.get_component_by_class(unreal.CameraComponent)
 p1.set_actor_location(unreal.Vector(-300,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 yield from wait(1.2)
 L=[]
 L.append('melee arm=%d off=(%d,%d,%d) fov=%d'%(boom.target_arm_length,boom.socket_offset.x,boom.socket_offset.y,boom.socket_offset.z,cam.field_of_view))
 p1.set_actor_rotation(unreal.Rotator(0,180,0),False)
 yield from wait(.2)
 yaw0=pc.get_control_rotation().yaw
 tgt=math.degrees(math.atan2(p2.get_actor_location().y-p1.get_actor_location().y,p2.get_actor_location().x-p1.get_actor_location().x))
 yield from wait(2.0)
 yaw1=pc.get_control_rotation().yaw
 moved=math.degrees(abs(math.atan2(math.sin(math.radians(yaw1-yaw0)),math.cos(math.radians(yaw1-yaw0)))))
 err=math.degrees(abs(math.atan2(math.sin(math.radians(tgt-yaw1)),math.cos(math.radians(tgt-yaw1)))))
 L.append('softlock moved=%.1f remaining_err=%.1f'%(moved,err))
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(1.2)
 L.append('ranged arm=%d off=(%d,%d,%d) fov=%d'%(boom.target_arm_length,boom.socket_offset.x,boom.socket_offset.y,boom.socket_offset.z,cam.field_of_view))
 p1.set_aim_intent(True);yield from wait(1.2)
 L.append('aim arm=%d off=(%d,%d,%d) fov=%d'%(boom.target_arm_length,boom.socket_offset.x,boom.socket_offset.y,boom.socket_offset.z,cam.field_of_view))
 p1.set_aim_intent(False)
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.get_targeting().lock_best_target()
 p1.set_actor_rotation(unreal.Rotator(0,180,0),False)
 yield from wait(1.2)
 fwd=math.atan2(p1.get_actor_forward_vector().y,p1.get_actor_forward_vector().x)
 t2=math.degrees(math.atan2(p2.get_actor_location().y-p1.get_actor_location().y,p2.get_actor_location().x-p1.get_actor_location().x))
 L.append('hardlock face_err=%.1f'%math.degrees(abs(math.atan2(math.sin(math.radians(t2-fwd)),math.cos(math.radians(t2-fwd))))))
 p1.get_targeting().clear_target()
 unreal.log('CAMRES2 '+json.dumps(L))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('CAMERR2 '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
