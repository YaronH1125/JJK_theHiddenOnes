
import unreal,json,math
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def suite():
 p1=gm.get_player_fighter()
 gm.reset_training();yield from wait(1)
 p1=gm.get_player_fighter()
 boom=p1.get_component_by_class(unreal.SpringArmComponent)
 cam=p1.get_component_by_class(unreal.CameraComponent)
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
 p1.set_aim_intent(True);yield from wait(1.5)
 sep=math.degrees(math.atan2(boom.socket_offset.y,boom.target_arm_length))
 unreal.log('M13RES2 aim arm=%d off=(%d,%d,%d) fov=%d sep=%.1f'%(boom.target_arm_length,boom.socket_offset.x,boom.socket_offset.y,boom.socket_offset.z,cam.field_of_view,sep))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M13ERR2 '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
