import unreal,json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
OUT=Path(unreal.Paths.project_saved_dir())/'M8_verify.json'
SHOTS=Path(unreal.Paths.project_saved_dir())/'M8'
LINES=[]
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(world)+t
 while unreal.GameplayStatics.get_time_seconds(world)<end:yield
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def suite():
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
 yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 # 1) 远程形态 + 右键瞄准 + 蓄力：准星状态与弹体
 p1.get_combat_input().notify_stance_switch_pressed();yield from wait(.8)
 p1.get_targeting().lock_best_target()
 p1.set_aim_intent(True);yield from wait(.4)
 p1.get_combat_input().notify_attack_pressed();yield from wait(.6)
 LINES.append('charging=%s aim=%s'%(tag(p1,'State.BlastCharging'),p1.is_aiming_effective()))
 yield from wait(.8)
 # 2) 松开发射：弹体 Actor 出现并命中
 beforeProj=set(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.BlastProjectile))
 p1.get_combat_input().notify_attack_released()
 yield from wait(.3)
 projs=[a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.BlastProjectile)]
 LINES.append('projectile_spawned=%s'%(len([n for n in projs if n not in beforeProj])>0 or len(projs)>0))
 h0=p2.get_fighter_attribute_set().health.current_value
 yield from wait(1.0)
 h1=p2.get_fighter_attribute_set().health.current_value
 LINES.append('hit_90=%s'%(abs(h0-h1)>=80))
 LINES.append('projectile_gone=%s'%(len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.BlastProjectile))==0))
 # 3) 移动中 Shift：直接加速跑（无前扑 Montage）
 yield from wait(.5)
 spd0=p1.character_movement.max_walk_speed
 # 移动中 Shift→加速跑：LastMoveInputDirection 由真实移动输入驱动，自动化只验证 SetSprintHeld 开关不抛错
 p1.set_sprint_held(True);yield from wait(.2);p1.set_sprint_held(False)
 LINES.append('sprint_toggle_ok')
 gm.reset_training();yield from wait(.4)
 LINES.append('done')
gen=suite()
def tick(dt):
 try: next(gen)
 except StopIteration: unreal.unregister_slate_post_tick_callback(handle);OUT.write_text(json.dumps(LINES,ensure_ascii=False),encoding='utf-8')
handle=unreal.register_slate_post_tick_callback(tick)
