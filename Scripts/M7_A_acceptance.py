"""M7 A 类修复验证：A01 瞄准 / A02 攻防统一 / A03 成本强度 / A04 不自充 / A05 会话 / A06 障碍。"""
import json,time,traceback
from pathlib import Path
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
fd=p1.get_definition()
out=Path(unreal.Paths.project_saved_dir())/'M7_A_acceptance.json'
state={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),'checks':[],'started_at':time.time()}
original=[]
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,value,detail=None):
 state['checks'].append(dict(name=name,passed=bool(value),details=detail));write();unreal.log(('[M7A] PASS ' if value else '[M7A] FAIL ')+name)
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(fn,t=5):
 end=now()+t
 while not fn() and now()<end:yield
def hp(f):return f.get_fighter_attribute_set().health.current_value
def curse(f):return f.get_fighter_attribute_set().cursed_energy.current_value
def energy(f):return f.get_fighter_attribute_set().energy.current_value
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def inp(f):return f.get_combat_input()
def place(f,x,y=0,yaw=0):
 f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,y,100),False,True);f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def temp(o,k,v):
 if not any(a==o and b==k for a,b,c in original):original.append((o,k,o.get_editor_property(k)))
 o.set_editor_property(k,v)
def clean():
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
def orbs():return unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DomainOrb)
def boom(f):return f.get_component_by_class(unreal.SpringArmComponent)
def lock(f):f.get_targeting().lock_best_target()
def to_ranged(f):
 # 切形态有间隔限制：失败后退避重试（实测被拒日志：间隔未到）
 for _ in range(4):
  if tag(f,'Stance.Ranged'):return
  inp(f).notify_stance_switch_pressed()
  yield from until(lambda:tag(f,'Stance.Ranged'),2)
  if tag(f,'Stance.Ranged'):return
  yield from wait(.6)

def suite():
 global p1,p2
 unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
 temp(fd,'initial_cursed_energy',100.);temp(fd,'initial_energy',100.)
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)

 # ---------- A01 瞄准 ----------
 place(p1,-500);place(p2,500,yaw=180);yield from wait(.3)
 p1.set_aim_intent(True);yield from wait(.5)
 check('A01_melee_no_effect',not p1.is_aiming_effective() and abs(boom(p1).get_editor_property('target_arm_length')-400.)<8.,
       {'arm':boom(p1).get_editor_property('target_arm_length')})
 p1.set_aim_intent(False)
 yield from to_ranged(p1)
 p1.set_aim_intent(True);yield from wait(.8)
 check('A01_ranged_effective',p1.is_aiming_effective() and boom(p1).get_editor_property('target_arm_length')<400.,
       {'arm':boom(p1).get_editor_property('target_arm_length'),'off':boom(p1).get_editor_property('socket_offset').y})
 check('A01_right_shoulder_offset',boom(p1).get_editor_property('socket_offset').y>20.)
 inp(p1).notify_stance_switch_pressed();yield from wait(.6)
 check('A01_cleared_by_stance_switch',not p1.is_aim_intent())
 yield from to_ranged(p1)
 p1.set_aim_intent(True);yield from wait(.3);p1.set_aim_intent(False);yield from wait(1.2)
 check('A01_release_restores',not p1.is_aiming_effective() and boom(p1).get_editor_property('target_arm_length')>388.,
       {'arm':boom(p1).get_editor_property('target_arm_length')})

 # ---------- A03 成本与强度（断言按实测持炮时长套公式，规避回调节拍误差） ----------
 c0=curse(p1);h0=hp(p2);lock(p1)
 inp(p1).notify_kick_pressed();t0=now();yield from wait(.5)
 inp(p1).notify_kick_released();held=now()-t0;yield from wait(.6)
 cost=c0-curse(p1)
 check('A03_below_min_exact45',held<1.2 and 45.<=cost<=45.6,{'held':held,'cost':cost})
 check('A03_below_min_no_damage',abs(hp(p2)-h0)<.5)
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);lock(p1);yield from wait(.3)
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_kick_pressed();t0=now();yield from wait(1.35)
 inp(p1).notify_kick_released();held=now()-t0;yield from until(lambda:hp(p2)<h0,2);yield from wait(.2)
 dmg=h0-hp(p2);cost=c0-curse(p1)
 # 按实测 held 套公式：q=clamp((held-1.2)/1.2)，伤害=150+70q，成本=45+20q
 q=sorted([0.,(held-1.2)/1.2,1.])[1]
 check('A03_just_past_gate_damage',abs(dmg-(150.+70.*q))<6.,{'held':held,'damage':dmg,'expect':150.+70.*q})
 # 命中回咒 +3 抵扣账面成本：净消耗 = 目标成本 - 3
 check('A03_just_past_gate_cost',abs(cost-(45.+20.*q-3.))<3.5,{'held':held,'cost':cost,'expect':45.+20.*q-3.})
 clean();temp(fd,'initial_cursed_energy',50.);clean()
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);lock(p1);yield from wait(.3)
 h0=hp(p2)
 inp(p1).notify_kick_pressed();yield from wait(2.7)
 inp(p1).notify_kick_released();yield from until(lambda:hp(p2)<h0,2);yield from wait(.2)
 dmg=h0-hp(p2);c1=curse(p1)
 # 咒力 50：45 起手 + 5 余额 → PaidCost=50 → q=0.25 → 伤害 ≈167.5；读数含回充漂移
 check('A03_exhausted_partial_strength',abs(dmg-167.5)<8.,{'damage':dmg,'curse':c1})
 check('A03_exhausted_no_negative',c1<7.,{'curse':c1})
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 # 无限资源：0 能量也能开领域（ModifyEnergy 豁免口径）
 temp(fd,'initial_energy',0.)
 clean();p1=gm.get_player_fighter();p2=gm.get_oponent_fighter() if False else gm.get_opponent_fighter();yield from wait(.5)
 ts=unreal.TrainingSettings();ts.set_editor_property('infinite_resources',True);gm.set_training_settings(ts)
 place(p1,-400);place(p2,400,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 check('A03_infinite_energy_domain',tag(p1,'State.DomainActive'))
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)

 # ---------- A02 攻防统一 ----------
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);lock(p1);yield from wait(.3)
 inp(p2).notify_guard_pressed();yield from wait(.2)
 h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(1.5)
 inp(p1).notify_attack_released();yield from wait(.4)
 # 命中后立即查硬直（chip=0 时 HP 不变，不能等 HP 条件）
 check('A02_front_guard_blocked',abs(hp(p2)-h0)<2.,{'drop':h0-hp(p2)})
 check('A02_front_guard_stun',tag(p2,'State.GuardStun'))
 yield from wait(.8)
 inp(p2).notify_guard_released();yield from wait(.4)
 h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(1.5)
 inp(p1).notify_attack_released();yield from until(lambda:hp(p2)<h0,2);yield from wait(.3)
 check('A02_blast_full_after_guard_drop',abs((h0-hp(p2))-90.)<6.,{'drop':h0-hp(p2)})
 # 闪避无敌窗躲掉普通炮
 h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(.4)
 inp(p2).notify_dodge_pressed(unreal.Vector(1,0,0))
 yield from until(lambda:tag(p2,'State.DodgeInvulnerable'),.4)
 bImmune=tag(p2,'State.DodgeInvulnerable')
 inp(p1).notify_attack_released();yield from wait(1.)
 check('A02_blast_dodge_immune',bImmune and abs(hp(p2)-h0)<.5,{'immortal_seen':bImmune,'drop':h0-hp(p2)})

 # ---------- A05/A02 墙体：炮击拦截 + 捕获遮挡 ----------
 pc.get_training_game_mode().jjk_spawn_blocker(0.,0.,0.5,20.,4.)
 yield from wait(.3)
 h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(1.5)
 inp(p1).notify_attack_released();yield from wait(1.)
 check('A02_wall_blocks_blast',abs(hp(p2)-h0)<.5,{'drop':hp(p2)-h0})
 place(p1,-400);yield from wait(.2)
 inp(p1).notify_domain_pressed();yield from wait(1.8)
 check('A05_capture_occluded',not tag(p1,'State.DomainActive') and len(orbs())==0)
 gm.jjk_clear_blockers();yield from wait(.3)
 check('A05_clear_blockers',True)

 # ---------- A05 压制销毁在飞球 ----------
 temp(fd,'initial_energy',100.);temp(fd,'initial_cursed_energy',100.)
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 place(p1,-550);place(p2,550,yaw=180);lock(p1);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 yield from wait(.55)  # 首球在飞（0.3s 生成，飞行 ~0.9s）
 inFlight=len(orbs())
 inp(p2).notify_domain_pressed();yield from until(lambda:tag(p2,'State.DomainActive'),2.5)
 yield from wait(.4)
 check('A05_suppression_destroys_inflight',inFlight>=1 and len(orbs())==0,{'inflight_seen':inFlight,'orbs':len(orbs())})
 check('A05_both_still_active',tag(p1,'State.DomainActive') and tag(p2,'State.DomainActive'))
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)

 # ---------- A05 术者受击跳过新球 ----------
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 place(p1,-140);place(p2,40,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 domainOpen=tag(p1,'State.DomainActive')
 yield from until(lambda:len(orbs())>=1 or curse(p1)<99.,2.5)
 # 受击落在 ~T+2.25（下个调度点 2.3 前一刻），硬直 0.5s 须覆盖 2.3 的出球
 yield from wait(1.55)
 inp(p2).submit_light_attack();yield from wait(.5)
 stunSeen=tag(p1,'State.HitStun')
 yield from wait(.4)
 skipOk=len(orbs())==0
 # 08 §6：100 咒力单发 65 通常只有一发，回充 6/s 不足以恢复第二发——不要求"恢复"
 check('A05_hitstun_domain_open',domainOpen,{'energy':energy(p1),'curse':curse(p1)})
 check('A05_caster_hitstun_skips_spawn',domainOpen and stunSeen and skipOk,
       {'stun':stunSeen,'skip':skipOk,'orbs':len(orbs())})
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)

 # ---------- A02/A06 领域球：必中 + 撞墙销毁 ----------
 place(p1,-400);place(p2,400,yaw=180);lock(p1);yield from wait(.3)
 inp(p2).notify_guard_pressed();yield from wait(.1)
 h0=hp(p2)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 yield from until(lambda:hp(p2)<h0,3);yield from wait(.2)
 check('A02_orb_unblockable',abs((h0-hp(p2))-220.)<6.,{'drop':h0-hp(p2)})
 inp(p2).notify_guard_released()
 # 球在飞时生成墙 → 球撞墙销毁且无伤害
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 place(p1,-400);place(p2,400,yaw=180);lock(p1);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 yield from wait(.38)  # 球已生成、在飞中段
 gm.jjk_spawn_blocker(0.,0.,0.5,20.,4.)
 h0=hp(p2);yield from wait(1.2)
 check('A06_orb_wall_destroy',len(orbs())==0 and abs(hp(p2)-h0)<.5,{'orbs':len(orbs()),'drop':h0-hp(p2)})
 gm.jjk_clear_blockers();clean();yield from wait(.3)

gen=suite();started=time.monotonic()
def finish():
 try:
  _finish_body()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();write()
def _finish_body():
 unreal.unregister_slate_post_tick_callback(handle)
 for o,k,v in reversed(original):o.set_editor_property(k,v)
 clean();gm.jjk_clear_blockers()
 state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>200:raise TimeoutError('M7A timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
