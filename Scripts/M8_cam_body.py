
import unreal,json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 yield from wait(1)
 p1=gm.get_player_fighter()
 boom=p1.get_component_by_class(unreal.SpringArmComponent)
 cam=p1.get_component_by_class(unreal.CameraComponent)
 L=[]
 L.append('idle arm=%d off=%s fov=%d'%(boom.target_arm_length,[round(boom.socket_offset.x,1),round(boom.socket_offset.y,1),round(boom.socket_offset.z,1)],cam.field_of_view))
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.set_aim_intent(True);yield from wait(1.5)
 L.append('aim arm=%d off=%s fov=%d'%(boom.target_arm_length,[round(boom.socket_offset.x,1),round(boom.socket_offset.y,1),round(boom.socket_offset.z,1)],cam.field_of_view))
 p1.set_aim_intent(False);yield from wait(2)
 L.append('restore arm=%d fov=%d'%(boom.target_arm_length,cam.field_of_view))
 unreal.log('CAMRES '+json.dumps(L))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('CAMERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
