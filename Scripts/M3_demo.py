# -*- coding: utf-8 -*-
"""带画面闭环与剩余边界检查；站位由调试脚本调整，所有动作走共享请求。"""
import unreal,json,time,traceback
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p1,p2=gm.get_player_fighter(),gm.get_opponent_fighter();fd=p1.get_definition()
a1=unreal.load_asset('/Game/Training/DA_M3_A1');kick=unreal.load_asset('/Game/Training/DA_M3_Kick')
report=Path(unreal.Paths.project_saved_dir())/'M3_demo.json'
evidence=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))/'Docs/开发过程/验收记录'
state={'status':'running','command_line':unreal.SystemLibrary.get_command_line(),'checks':[],'stages':[],'frames':[]}
original=[]
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(pred,t=1.2):
 end=now()+t
 while not pred() and now()<end:yield
def hp(f):return f.get_fighter_attribute_set().get_editor_property('health').current_value
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def move(f,x,yaw):
 f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,0,100),False,True)
 f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def near():move(p1,-50,0);move(p2,50,180)
def reset():gm.reset_training();near()
def check(n,c,d=None):
 state['checks'].append(dict(name=n,passed=bool(c),details=d));write()
def write():report.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def stage(n):
 state['stages'].append(dict(stage=n,time=now(),p1_hp=hp(p1),p2_hp=hp(p2)));write()
def capture(n):
 path=(evidence/('M3_'+n+'.png')).as_posix()
 unreal.SystemLibrary.execute_console_command(w,'HighResShot 1280x720 filename="'+path+'"')
def edit(obj,k,v):
 original.append((obj,k,obj.get_editor_property(k)));obj.set_editor_property(k,v)
def suite():
 pc.set_control_rotation(unreal.Rotator(pitch=-18,yaw=-35,roll=0))
 gm.call_method('JJKDebugHud');unreal.SystemLibrary.execute_console_command(w,'JJK.DebugHitFX 1')
 reset();yield from wait(.5)
 # 三段输入按实际段数等待，避免把机器壁钟时间当作动作时间。
 p1.get_combat_input().submit_light_attack();yield from wait(.3);p1.get_combat_input().submit_light_attack()
 yield from until(lambda:p1.get_combat_hit().get_segment_id()==1)
 yield from wait(.3);p1.get_combat_input().submit_light_attack()
 yield from until(lambda:p1.get_combat_hit().get_segment_id()==2)
 yield from wait(.5);stage('三段拳击');check('demo_three_hits',hp(p2)==870,hp(p2))
 yield from wait(.8);near();yield from wait(.3)
 p2.get_combat_input().notify_guard_pressed();p1.get_combat_input().submit_kick()
 yield from until(lambda:tag(p2,'State.GuardStun'))
 check('demo_guard',hp(p2)==870 and tag(p2,'State.GuardStun'));stage('正面防御')
 yield from wait(1.1);p1.get_combat_input().submit_light_attack()
 yield from until(lambda:p1.is_throw_paired());check('demo_throw_started',p1.is_throw_paired())
 stage('条件投技');capture('投技占位演示')
 yield from until(lambda:not p1.is_throw_paired(),2.)
 check('demo_throw_released',p1.can_act() and p2.can_act() and hp(p2)==750,hp(p2))
 near();yield from wait(.3);p2.get_combat_input().submit_kick();yield from wait(.18)
 check('demo_dodge',p1.request_dodge(unreal.Vector(0,-1,0)));stage('对手出招与方向闪避');yield from wait(1.2)
 near();yield from wait(.3);p1.get_combat_input().notify_guard_pressed();p2.get_combat_input().submit_kick()
 yield from until(lambda:tag(p1,'State.GuardStun'));check('demo_player_guard',hp(p1)==1000,hp(p1))
 p1.get_combat_input().notify_guard_released();yield from wait(1.1)
 p1.get_combat_input().submit_light_attack();yield from wait(.2)
 check('demo_legal_cancel',p1.request_dodge(unreal.Vector(0,1,0)) and not p1.get_combat_hit().has_active_attack());stage('合法取消')
 yield from wait(.8);near();yield from wait(.3)
 p1.get_combat_input().submit_heavy_kick();yield from until(lambda:tag(p2,'State.KnockedDown'))
 check('demo_knockdown',tag(p2,'State.KnockedDown'));stage('击退倒地');capture('倒地占位演示')
 yield from until(lambda:tag(p2,'State.GettingUp'),1.5)
 check('demo_getup_protected',tag(p2,'State.GettingUp') and not p2.can_act())
 yield from wait(.6);check('demo_recovered',p2.can_act())
 # 以真实重拳 GE 伤害打到死亡，不调用调试 Kill。
 for _ in range(12):
  if p2.is_dead():break
  near();yield from wait(.3);p1.get_combat_input().submit_heavy_punch();yield from wait(1.15)
 check('demo_real_damage_death',p2.is_dead() and hp(p2)==0);stage('真实伤害致死')
 check('demo_dead_cannot_escape',not p2.request_dodge(unreal.Vector()) and not p2.request_stance_switch())
 reset();yield from wait(.3);p1.get_combat_input().submit_light_attack();yield from wait(1.2)
 check('demo_reset_reattack',hp(p2)==965 and not p1.is_dead());stage('重置再次出招')
 # T09 缺配对动画/距离不足：仍防御，不丢伤害结果或锁人。
 reset();yield from wait(.3)
 cfg=fd.get_editor_property('throw_config');cfg.set_editor_property('max_distance',80.);edit(fd,'throw_config',cfg)
 p2.get_combat_input().notify_guard_pressed();p1.get_combat_input().submit_light_attack();yield from wait(.7)
 check('edge_throw_too_far',hp(p2)==1000 and not p1.is_throw_paired())
 cfg.set_editor_property('max_distance',180.);fd.set_editor_property('throw_config',cfg)
 reset();yield from wait(.3);edit(a1,'hit_react_montage',None)
 p2.get_combat_input().notify_guard_pressed();p1.get_combat_input().submit_light_attack();yield from wait(.7)
 check('edge_missing_pair_animation_fallback',hp(p2)==1000 and not p1.is_throw_paired())
 a1.set_editor_property('hit_react_montage',unreal.load_asset('/Game/Training/AM_M2_HitReact'))
 reset();yield from wait(.3)
 # 空中目标保持在可接触高度，确保检验的是抓取地面限制，而不是挥空。
 gravity=p2.character_movement.gravity_scale
 p2.character_movement.set_editor_property('gravity_scale',0.)
 p2.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FALLING)
 p2.set_actor_location(unreal.Vector(50,0,150),False,True)
 p2.get_combat_input().notify_guard_pressed();p1.get_combat_input().submit_light_attack()
 yield from until(lambda:tag(p2,'State.GuardStun'))
 check('edge_airborne_throw_falls_back',hp(p2)==1000 and not p1.is_throw_paired() and tag(p2,'State.GuardStun'))
 p2.character_movement.set_editor_property('gravity_scale',gravity)
 # 非可闪避类型穿过无敌；恢复期可被普通攻击打断。固定零位移只为保证接触。
 cfg=fd.get_editor_property('dodge_config');cfg.set_editor_property('dodge_speed',0.);edit(fd,'dodge_config',cfg)
 reset();yield from wait(.3);edit(kick,'bDodgeable',False)
 p1.get_combat_input().submit_kick();yield from wait(.28);p2.request_dodge(unreal.Vector())
 yield from wait(.6);check('edge_undodgeable_type',hp(p2)==955,hp(p2));kick.set_editor_property('bDodgeable',True)
 reset();yield from wait(.3);p2.request_dodge(unreal.Vector());yield from wait(.08);p1.get_combat_input().submit_kick()
 yield from wait(.6);check('edge_hit_during_dodge_recovery',hp(p2)==955 and not tag(p2,'State.DodgeRecovery'),hp(p2))
 cfg.set_editor_property('dodge_speed',900.);fd.set_editor_property('dodge_config',cfg)
 reset();yield from wait(.3);p1.get_combat_input().submit_light_attack();yield from wait(.15)
 before=p1.get_actor_location();p1.do_move(1.,1.);p1.jump();yield from wait(.12)
 after=p1.get_actor_location();check('edge_movement_locked',abs(after.x-before.x)<2 and abs(after.y-before.y)<2 and abs(after.z-before.z)<2)
 reset();yield from wait(.3);pc.set_combat_input_enabled(False)
 check('edge_menu_shared_requests_blocked','REJECTED_BLOCKED' in str(p1.get_combat_input().submit_light_attack()) and not p1.request_dodge(unreal.Vector()))
 pc.set_combat_input_enabled(True);reset();yield from wait(.3);capture('擂台与调试状态')
 yield from wait(.5)
gen=suite();start=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 for obj,k,v in reversed(original):obj.set_editor_property(k,v)
 # 可编辑结构是引用包装，显式恢复用于探针的字段。
 p2.character_movement.set_editor_property('gravity_scale',1.)
 cfg=fd.get_editor_property('dodge_config');cfg.set_editor_property('dodge_speed',900.);fd.set_editor_property('dodge_config',cfg)
 cfg=fd.get_editor_property('throw_config');cfg.set_editor_property('max_distance',180.);fd.set_editor_property('throw_config',cfg)
 gm.reset_training();pc.set_combat_input_enabled(True)
 state['duration_seconds']=time.monotonic()-start;write()
def tick(dt):
 try:
  if time.monotonic()-start>150:raise TimeoutError('demo timeout')
  state['frames'].append(unreal.GameplayStatics.get_world_delta_seconds(w));next(gen)
 except StopIteration:
  state['status']='passed' if all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
print('M3 rendered demo started')
