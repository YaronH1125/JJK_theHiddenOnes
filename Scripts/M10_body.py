
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
 L=[]
 # 1) 切换不断移动：给角色一个初速度（模拟移动中），切换期间位移应保持
 p1.set_actor_location(unreal.Vector(-300,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 p1.character_movement.velocity=unreal.Vector(600,0,0)
 p1.get_combat_input().notify_stance_switch_pressed()
 x0=p1.get_actor_location().x
 yield from wait(.35)
 moved=p1.get_actor_location().x-x0
 L.append('switch_kept_moving=%s moved=%.0f'%(moved>100,moved))
 gm.reset_training();yield from wait(.8)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 # 2) 远程开火朝向：角色背对镜头方向 → 蓄力期间平滑转向
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.set_actor_location(unreal.Vector(-300,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 p1.set_actor_rotation(unreal.Rotator(0,180,0),False)
 yield from wait(.2)
 p1.get_combat_input().notify_attack_pressed()
 yield from wait(.8)
 fwd=math.atan2(p1.get_actor_forward_vector().y,p1.get_actor_forward_vector().x)
 err=math.degrees(abs(math.atan2(math.sin(math.radians(0-fwd)),math.cos(math.radians(0-fwd)))))
 L.append('fire_face_err=%.1f'%err)
 p1.get_combat_input().notify_attack_released();yield from wait(1.2)
 # 3) 磁吸：350 内攻击距离外，出招面向 + 滑步贴近
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.set_actor_location(unreal.Vector(-280,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 p1.set_actor_rotation(unreal.Rotator(0,0,0),False)
 yield from wait(.2)
 x0=p1.get_actor_location().x
 p1.get_combat_input().submit_light_attack()
 yield from wait(.5)
 x1=p1.get_actor_location().x
 L.append('magnetism lunge=%.0f'%(x1-x0))
 unreal.log('M10RES '+json.dumps(L))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M10ERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
