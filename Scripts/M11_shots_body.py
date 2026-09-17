
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
 p1.set_actor_location(unreal.Vector(-300,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 p1.get_targeting().lock_best_target()
 # 远程过肩瞄准 + 蓄力（准星+弹体）
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.set_aim_intent(True);yield from wait(1.0)
 p1.get_combat_input().notify_attack_pressed();yield from wait(.8)
 unreal.AutomationLibrary.take_high_res_screenshot(1280,720,'M11_ranged_aim')
 yield from wait(1.0)
 p1.get_combat_input().notify_attack_released();yield from wait(1.0)
 p1.set_aim_intent(False)
 # 近战居中视角出招
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 unreal.AutomationLibrary.take_high_res_screenshot(1280,720,'M11_melee_center')
 yield from wait(.4)
 unreal.log('M11 shots taken')
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M11ERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
