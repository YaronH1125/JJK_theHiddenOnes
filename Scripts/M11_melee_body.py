
import unreal
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
 p1.set_actor_location(unreal.Vector(-300,0,100),False,True)
 p2.set_actor_location(unreal.Vector(300,0,100),False,True)
 p1.set_actor_rotation(unreal.Rotator(0,0,0),False)
 yield from wait(.3)
 unreal.AutomationLibrary.take_high_res_screenshot(1280,720,'M11_melee_center')
 yield from wait(1.5)
 unreal.log('M11 melee shot done')
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M11ERR2 '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
