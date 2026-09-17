"""M6 在真实 PIE 中验证蓄力炮/领域展开/咒球/AI 远程分支的自动化子集。

覆盖 M6-T04/T05/T07/T08/T09/T10/T11/T16/T20/T22/T23 中可程序化断言的路径；
纯手感动效（T01–T03/T06/T12–T15/T17–T19 的人工部分）按验收记录模板另行登记。
"""
import json,time,traceback,math
from pathlib import Path
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
fd=p1.get_definition()
out=Path(unreal.Paths.project_saved_dir())/'M6_acceptance.json'
state={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),'checks':[],'started_at':time.time()}
original=[]
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,value,detail=None):
 state['checks'].append(dict(name=name,passed=bool(value),details=detail));write();unreal.log(('[M6] PASS ' if value else '[M6] FAIL ')+name)
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
def lock(f):
 f.get_targeting().lock_best_target()
def to_ranged(f):
 if not tag(f,'Stance.Ranged'):
  inp(f).notify_stance_switch_pressed();yield from until(lambda:tag(f,'Stance.Ranged'),2)

def suite():
 global p1,p2
 unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
 # ---------- 场景 A：移动炮曲线（M6-T04/T16） ----------
 temp(fd,'initial_cursed_energy',100.);temp(fd,'initial_energy',0.)
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)
 yield from to_ranged(p1)
 place(p1,-500);place(p2,500,yaw=180);lock(p1);yield from wait(.3)
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(.25)
 check('M6-T04_mobile_charging_tag',tag(p1,'State.BlastCharging') and tag(p1,'State.RangedBlastCharging'))
 inp(p1).notify_attack_released();yield from until(lambda:hp(p2)<h0,2);yield from wait(.2)
 dmg=h0-hp(p2);cost=c0-curse(p1)
 # q≈0.208：伤害 30+60q≈42.5，成本 8+16q≈11.3（允许时序抖动）
 check('M6-T04_mobile_partial_curve',30.<=dmg<=48.,{'damage':dmg,'cost':cost})
 check('M6-T04_mobile_partial_cost',8.<=cost<=14.,{'damage':dmg,'cost':cost})
 # 等上一发后摇结束（恢复期激活被拒），再满蓄
 yield from until(lambda:not tag(p1,'State.BlastCharging'),1.5);yield from wait(.2)
 # 满蓄封顶（M6-T16：持至封顶不发射，松开单发）
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(2.0)
 check('M6-T16_hold_cap_no_fire',abs(hp(p2)-h0)<.5,{'held_hp':hp(p2)})
 inp(p1).notify_attack_released();yield from until(lambda:hp(p2)<h0,2);yield from wait(.2)
 dmg=h0-hp(p2);cost=c0-curse(p1)
 check('M6-T04_mobile_full_damage',88.<=dmg<=92.,{'damage':dmg})
 check('M6-T16_full_cost_cap',22.<=cost<=24.5,{'cost':cost})
 # 咒力不足起手拒绝（M6-T04）
 clean()
 temp(fd,'initial_cursed_energy',5.);clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);yield from wait(.3)
 h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(.2)
 check('M6-T04_insufficient_no_charge',not tag(p1,'State.BlastCharging'))
 inp(p1).notify_attack_released();yield from wait(.5)
 check('M6-T04_insufficient_no_damage',abs(hp(p2)-h0)<.5)
 # ---------- 场景 B：超级炮门槛/冷却（M6-T05/T19） ----------
 clean()
 temp(fd,'initial_cursed_energy',100.);clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);yield from wait(.3)
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_kick_pressed();yield from wait(.5)
 inp(p1).notify_kick_released();yield from wait(.6)
 # 低于门槛：不发射、已扣不退、进入冷却
 check('M6-T05_below_min_no_damage',abs(hp(p2)-h0)<.5,{'hp':hp(p2)})
 check('M6-T05_below_min_cost_kept',c0-curse(p1)>=44.,{'cost':c0-curse(p1)})
 check('M6-T05_below_min_cooldown',tag(p1,'State.SuperBlastCooldown'))
 # 冷却通过训练重置清除（ClearTrainingCooldowns）
 clean();yield from wait(.5);p1=gm.get_player_fighter()
 check('M6-T19_cooldown_cleared_by_reset',not tag(p1,'State.SuperBlastCooldown'))
 yield from to_ranged(p1);place(p1,-500);place(p2,500,yaw=180);yield from wait(.3)
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_kick_pressed();yield from wait(2.7)
 inp(p1).notify_kick_released();yield from until(lambda:hp(p2)<h0,2);yield from wait(.2)
 dmg=h0-hp(p2)
 check('M6-T05_full_damage',215.<=dmg<=225.,{'damage':dmg})
 check('M6-T05_full_cost',63.<=c0-curse(p1)<=66.,{'cost':c0-curse(p1)})
 yield from until(lambda:tag(p1,'State.SuperBlastCooldown'),1.5)
 check('M6-T19_cooldown_after_fire',tag(p1,'State.SuperBlastCooldown'))
 h1=hp(p2)
 inp(p1).notify_kick_pressed();yield from wait(.2);inp(p1).notify_kick_released();yield from wait(.5)
 check('M6-T19_cooldown_blocks_second',abs(hp(p2)-h1)<.5,{'hp':hp(p2)})
 # ---------- 场景 C：领域捕获/打断（M6-T07） ----------
 clean()
 temp(fd,'initial_energy',100.);clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)
 place(p1,-900);place(p2,900,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from wait(1.8)
 check('M6-T07_out_of_range_rejected',not tag(p1,'State.DomainActive') and len(orbs())==0)
 # 结印中重置：无领域、无残留标签（M6-T07/T13 部分）
 clean();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(.5)
 place(p1,-400);place(p2,400,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from wait(.4)
 check('M6-T07_casting_tag',tag(p1,'State.DomainCasting'))
 gm.reset_training();yield from wait(.5);p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 check('M6-T07_interrupt_no_domain',not tag(p1,'State.DomainActive') and not tag(p1,'State.DomainCasting') and len(orbs())==0)
 # ---------- 场景 D：领域调度/咒球经济（M6-T20/T08/T11/T22/T23） ----------
 clean();temp(fd,'initial_energy',100.);temp(fd,'initial_cursed_energy',100.);clean()
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();yield from wait(1)
 place(p1,-400);place(p2,400,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 check('M6-T07_domain_open',tag(p1,'State.DomainActive'))
 # 出球前位移目标：球生成后即弧线修正追踪（M6-T08：普通移动不免疫）
 place(p2,150,120,180);yield from wait(.1)
 # 首球延迟 0.3s，成本 65 → 咒力 35（M6-T20）
 yield from until(lambda:curse(p1)<99.,2)
 check('M6-T20_orb_cost_charged',33.<=curse(p1)<=40.,{'curse':curse(p1)})
 yield from until(lambda:hp(p2)<999.9,3)
 check('M6-T08_orb_tracks_moved_target',hp(p2)<999.9,{'hp':hp(p2)})
 # 领域能量按命中获得（M6-T11：不自充，命中 +10 封顶）
 check('M6-T11_energy_gain_from_hit',4.<=energy(p1)<=10.,{'energy':energy(p1)})
 # 咒力不足跳过第二次出球（M6-T20：不补齐、无残留球）
 yield from wait(2.0)
 check('M6-T20_insufficient_skip',len(orbs())==0 and 33.<=curse(p1)<=40.1,{'orbs':len(orbs()),'curse':curse(p1)})
 # 领域期内手动远程炮禁用（M6-T23；先切远程形态再验证）
 yield from to_ranged(p1)
 inp(p1).notify_attack_pressed();yield from wait(.3)
 check('M6-T23_manual_blast_disabled',not tag(p1,'State.RangedBlastCharging'))
 inp(p1).notify_attack_released();yield from wait(.3)
 # 到期清场（M6-T09：固定时限；会话结束清标签/清球）
 yield from until(lambda:not tag(p1,'State.DomainActive'),7)
 check('M6-T09_domain_expiry',not tag(p1,'State.DomainActive'))
 yield from wait(.5)
 check('M6-T09_expiry_clears_orbs',len(orbs())==0,{'orbs':len(orbs())})
 # 领域结束后手动炮恢复（需新按下；M6-T23）
 c0=curse(p1);h0=hp(p2)
 inp(p1).notify_attack_pressed();yield from wait(.3)
 check('M6-T23_manual_blast_restored',tag(p1,'State.RangedBlastCharging'))
 inp(p1).notify_attack_released();yield from wait(.6)
 check('M6-T23_manual_blast_fires_after',curse(p1)<c0)
 # ---------- 场景 E：双领域压制（M6-T10/T09） ----------
 clean();yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 place(p1,-400);place(p2,400,yaw=180);yield from wait(.3)
 inp(p1).notify_domain_pressed();yield from until(lambda:tag(p1,'State.DomainActive'),2.5)
 inp(p2).notify_domain_pressed();yield from until(lambda:tag(p2,'State.DomainActive'),2.5)
 check('M6-T10_dual_active',tag(p1,'State.DomainActive') and tag(p2,'State.DomainActive'))
 yield from wait(1.2)
 # 压制期：无在飞球、无新生成
 check('M6-T10_suppressed_no_orbs',len(orbs())==0,{'orbs':len(orbs())})
 # 先开的一方先到期 → 剩余一方按原调度恢复
 yield from until(lambda:not tag(p1,'State.DomainActive'),8)
 check('M6-T10_first_expires',not tag(p1,'State.DomainActive') and tag(p2,'State.DomainActive'))
 yield from until(lambda:len(orbs())>=1,3)
 check('M6-T10_remaining_resumes',len(orbs())>=1,{'orbs':len(orbs())})
 clean();yield from wait(.5)
 # ---------- 场景 F：AI 远程分支（M6-T14 部分） ----------
 gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();yield from wait(1)
 p=ai.get_params();p.set_editor_property('enable_ranged_combat',True);p.set_editor_property('decision_interval',.05);ai.set_params(p)
 place(p1,-700);place(p2,700,yaw=180);yield from wait(.5)
 # AI 远程链路：共享入口切形态（合法校验与玩家一致），再固定 RangedAttack 分支
 inp(p2).notify_stance_switch_pressed();yield from until(lambda:tag(p2,'Stance.Ranged'),2)
 check('M6-T14_ai_switched_ranged',tag(p2,'Stance.Ranged'))
 ai.debug_request_branch(unreal.AIBranch.RANGED_ATTACK)
 h0=hp(p1)
 yield from until(lambda:hp(p1)<h0-10,6)
 check('M6-T14_ai_ranged_hit',hp(p1)<h0-10,{'hp':hp(p1),'log':list(ai.get_decision_log())[-8:]})
 check('M6-T14_ai_ranged_branch',any('RangedAttack' in x for x in list(ai.get_decision_log())))
 clean();yield from wait(.3)

gen=suite();started=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 for o,k,v in reversed(original):o.set_editor_property(k,v)
 clean()
 state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>240:raise TimeoutError('M6 timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
