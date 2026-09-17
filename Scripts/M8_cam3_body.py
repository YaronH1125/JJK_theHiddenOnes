
import unreal,json,math
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 gm.reset_training();yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 # 镜头转背（模拟玩家把镜头转离目标），随后静置观察柔性回正
 pc.set_control_rotation(unreal.Rotator(0,0,180))
 yield from wait(.2)
 yaw0=pc.get_control_rotation().yaw
 tgt=math.degrees(math.atan2(p2.get_actor_location().y-p1.get_actor_location().y,p2.get_actor_location().x-p1.get_actor_location().x))
 yield from wait(2.5)
 yaw1=pc.get_control_rotation().yaw
 moved=math.degrees(abs(math.atan2(math.sin(math.radians(yaw1-yaw0)),math.cos(math.radians(yaw1-yaw0)))))
 err=math.degrees(abs(math.atan2(math.sin(math.radians(tgt-yaw1)),math.cos(math.radians(tgt-yaw1)))))
 unreal.log('CAMRES3 drifted=%.1f remaining_err=%.1f'%(moved,err))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('CAMERR3 '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
