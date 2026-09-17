
import unreal,json
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
 p1.get_combat_input().notify_stance_switch_pressed()
 x0=p1.get_actor_location().x
 # 逐帧刷新速度=模拟持续按住 W（摩擦不会吃掉输入驱动）
 switching=True
 while switching and p1.get_actor_location().x-x0<150:
  p1.character_movement.velocity=unreal.Vector(600,0,0)
  yield
 moved=p1.get_actor_location().x-x0
 unreal.log('M10RES2 switch_moved=%.0f'%moved)
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M10ERR2 '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
