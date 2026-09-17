
import unreal,json,math
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.reset_training();yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 L=[]
 # 1) 连按 E 快速来回切：两次 0.05s 间隔连按，形态应翻两次回到原位
 st0=p1.get_stance()
 p1.get_combat_input().notify_stance_switch_pressed()
 yield from wait(.05)
 p1.get_combat_input().notify_stance_switch_pressed()
 yield from wait(.35)
 st1=p1.get_stance()
 L.append('rapid_toggle back_to_original=%s'%(st0==st1))
 # 2) 单次切换仍是即时生效
 p1.get_combat_input().notify_stance_switch_pressed()
 yield from wait(.15)
 L.append('instant_flip=%s'%(p1.get_stance()!=st0))
 # 3) 切换中移动保持：切形态 + 逐帧速度刷新
 p1.get_combat_input().notify_stance_switch_pressed()
 yield from wait(.05)
 p1.get_combat_input().notify_stance_switch_pressed()
 x0=p1.get_actor_location().x
 for _ in range(12):
  p1.character_movement.velocity=unreal.Vector(600,0,0);yield
 moved=p1.get_actor_location().x-x0
 L.append('switch_move=%.0f'%moved)
 # 4) 瞄准角分离 ≈28°
 boom=p1.get_component_by_class(unreal.SpringArmComponent)
 cam=p1.get_component_by_class(unreal.CameraComponent)
 L.append('aim arm=%d off=(%d,%d,%d) fov=%d sep=%.1f'%(boom.target_arm_length,boom.socket_offset.x,boom.socket_offset.y,boom.socket_offset.z,cam.field_of_view,math.degrees(math.atan2(boom.socket_offset.y,boom.target_arm_length))))
 unreal.log('M13RES '+json.dumps(L))
gen=suite()
def tick(dt):
 try: next(gen)
 except Exception as e: unreal.log('M13ERR '+str(e));unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
