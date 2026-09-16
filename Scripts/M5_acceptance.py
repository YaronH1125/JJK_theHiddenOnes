"""M5 在真实 PIE 中验证行为树、导航、共享动作和对局生命周期。"""
import json,time,traceback,math
from pathlib import Path
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
fd=p1.get_definition()
out=Path(unreal.Paths.project_saved_dir())/'M5_acceptance.json'
state={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),'checks':[],'started_at':time.time()}
original=[]
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,value,detail=None):
 state['checks'].append(dict(name=name,passed=bool(value),details=detail));write();unreal.log(('[M5] PASS ' if value else '[M5] FAIL ')+name)
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(fn,t=3):
 end=now()+t
 while not fn() and now()<end:yield
def hp(f):return f.get_fighter_attribute_set().health.current_value
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)
def inp(f):return f.get_combat_input()
def loc(f):return f.get_actor_location()
def dist():return (loc(p1)-loc(p2)).length()
def place(f,x,y=0,yaw=0):
 f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,y,100),False,True);f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def temp(o,k,v):
 if not any(a==o and b==k for a,b,c in original):original.append((o,k,o.get_editor_property(k)))
 o.set_editor_property(k,v)
def params(ai,**kw):
 p=ai.get_params()
 for k,v in kw.items():p.set_editor_property(k,v)
 ai.set_params(p)
def clean():
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
def suite():
 global p1,p2
 unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
 clean();yield from wait(1)
 check('M5-T01_enable',gm.set_opponent_mode(unreal.OpponentMode.AI));ai=gm.get_opponent_ai()
 yield from wait(1)
 check('M5-T01_native_tree',ai.is_ai_active() and ai.get_editor_property('arena_tree') is not None,ai.get_debug_state())
 check('M5-T01_blackboard_target',ai.get_component_by_class(unreal.BlackboardComponent).get_value_as_object('Target')==p1)
 check('M5-T02_nav_ready',ai.is_navigation_ready(),str(loc(p2)))
 check('M5-T02_capsule_agent',p2.capsule_component.get_scaled_capsule_radius()==42 and p2.capsule_component.get_scaled_capsule_half_height()==96)
 check('M5-T02_interior_coverage',all(ai.is_legal_destination(unreal.Vector(850*math.cos(a),850*math.sin(a),100)) for a in [i*math.pi/4 for i in range(8)]))
 check('M5-T02_reject_outside',not ai.is_legal_destination(unreal.Vector(1800,0,100)))
 yield from until(lambda:dist()<300,5)
 check('M5-T02_approach',dist()<300,dist())
 gm.set_opponent_mode(unreal.OpponentMode.FIXED_GUARD);yield from wait(.15)
 check('M5-T01_fixed_guard',p2.is_guard_intent() and gm.get_opponent_ai() is None)
 gm.set_opponent_mode(unreal.OpponentMode.STATIC);yield from wait(.15)
 start=loc(p2);yield from wait(.5)
 check('M5-T01_static_stop',not p2.is_guard_intent() and (loc(p2)-start).length()<1)
 clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();yield from wait(.7)
 pc.set_training_panel_open(True);yield from wait(.2);start=loc(p2)
 check('M5-T06_menu_tree_stopped',not ai.is_ai_active() and ai.get_action_binding_count()==0)
 yield from wait(.6);check('M5-T06_menu_no_movement',(loc(p2)-start).length()<1)
 pc.set_training_panel_open(False);yield from wait(.6)
 check('M5-T06_menu_resume',ai.is_ai_active())
 # 确定性近身：只选轻拳，观察真实共享 GAS/命中伤害。
 clean();place(p1,350);place(p2,500,yaw=180)
 gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai()
 params(ai,attack_chance=1.,heavy_chance=0.,retreat_distance=0.,random_seed=12,decision_interval=.15)
 before=hp(p1);yield from until(lambda:hp(p1)<before,4)
 check('M5-T07_shared_damage',hp(p1)<before,dict(before=before,after=hp(p1)))
 check('M5-T07_GAS_task',any('Request=Executed' in x for x in list(ai.get_decision_log())),list(ai.get_decision_log()))
 pc.set_training_panel_open(True);yield from wait(.1)
 check('M5-T06_task_abort_cleanup',ai.get_action_binding_count()==0 and not p2.is_attacking() and not p2.is_guard_intent())
 pc.set_training_panel_open(False)
 p2.jjk_debug_force_hit_react();yield from wait(.15)
 check('M5-T05_hit_stops_path',p2.get_velocity().length()<1, str(p2.get_velocity()))
 yield from wait(1.5);check('M5-T05_recovery',ai.is_ai_active())
 # 树任务边界复现仍走正式共享入口，不直接调用 GA Activate。
 for missing in [False,True]:
  clean();place(p1,-500);place(p2,500,yaw=180)
  gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai()
  params(ai,decision_interval=.05,attack_chance=0.,heavy_chance=0.,min_branch_hold_time=0.)
  if missing:temp(fd,'melee_attack_ability',None)
  else:p2.jjk_debug_force_hit_react()
  ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from wait(.3)
  text='Rejected: ability missing' if missing else 'Request=RejectedBlocked'
  check('M5-T03_'+('missing' if missing else 'hit_blocked'),any(text in x for x in ai.get_decision_log()),list(ai.get_decision_log()))
  check('M5-T04_rejection_cleanup_'+str(missing),ai.get_action_binding_count()==0)
  if missing:fd.set_editor_property('melee_attack_ability',next(v for o,k,v in original if o==fd and k=='melee_attack_ability'))
 # 行动资源不足的闪避经过相同规则拒绝；防御不依赖资源。
 temp(fd,'initial_action_resource',0.)
 clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05)
 ai.debug_request_branch(unreal.AIBranch.DODGE);yield from wait(.25)
 check('M5-T03_resource_rejected',any('Request=RejectedBlocked' in x for x in ai.get_decision_log()) and ai.get_action_binding_count()==0)
 ai.debug_request_branch(unreal.AIBranch.DEFEND);yield from wait(.2)
 check('M5-T03_guard_without_resource',p2.is_guard_intent())
 gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 check('M5-T06_guard_abort',not p2.is_guard_intent())
 fd.set_editor_property('initial_action_resource',5.)
 # 缓存消费、过期与被新请求覆盖三种路径。
 a1=unreal.load_asset('/Game/Training/DA_M3_A1')
 for key,value in [('combo_window_start_time',.75),('combo_window_end_time',.9),('allow_kick_transition',True),('allow_heavy_transition',True)]:temp(a1,key,value)
 for scenario in ['consumed','expired','superseded','abort']:
  temp(fd,'combo_cache_lifetime',.02 if scenario=='expired' else 1.5)
  clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05,attack_chance=0.,heavy_chance=0.,random_seed=12)
  inp(p2).submit_light_attack();ai.debug_request_branch(unreal.AIBranch.ATTACK)
  yield from until(lambda:any('Request=Cached' in x for x in ai.get_decision_log()),.3)
  check('M5-T04_cached_'+scenario,any('Request=Cached' in x for x in ai.get_decision_log()),list(ai.get_decision_log()))
  if scenario=='superseded':
   inp(p2).submit_heavy_punch();yield from wait(.1)
   check('M5-T04_keep_newer_cache',inp(p2).peek_cached_action()==unreal.CachedAction.HEAVY_PUNCH and ai.get_action_binding_count()==0)
  elif scenario=='abort':
   pc.set_training_panel_open(True)
   check('M5-T06_cached_abort',ai.get_action_binding_count()==0 and inp(p2).peek_cached_action()==unreal.CachedAction.NONE)
  else:
   yield from wait(2.6 if scenario=='consumed' else 1.6)
   expected='Cache consumed' if scenario=='consumed' else 'Cache expired'
   check('M5-T04_'+scenario,any(expected in x for x in ai.get_decision_log()),list(ai.get_decision_log()))
   check('M5-T04_task_ends_'+scenario,ai.get_action_binding_count()==0)
 for o,k,v in original:
  if o==a1 or (o==fd and k=='combo_cache_lifetime'):o.set_editor_property(k,v)
 # 用已授予的开发 Probe 验证瞬时完成及真实 GE 冷却，不宣称正式技能完成。
 temp(fd,'initial_energy',50.)
 probe=unreal.load_class(None,'/Script/JJK_theHiddenOnes.TrainingProbeAbility')
 temp(fd,'melee_attack_ability',probe)
 clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05,attack_chance=0.,heavy_chance=0.)
 before=p2.get_fighter_attribute_set().cursed_energy.current_value
 ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from wait(.2)
 check('M5-T04_instant_completion',any('Request=Executed' in x for x in ai.get_decision_log()) and ai.get_action_binding_count()==0 and p2.get_fighter_attribute_set().cursed_energy.current_value==before-10)
 ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from wait(.2)
 check('M5-T03_real_cooldown',any('Request=RejectedAbilityMissing' in x for x in ai.get_decision_log()) and ai.get_action_binding_count()==0)
 for o,k,v in original:
  if o==fd and k in ('melee_attack_ability','initial_energy'):o.set_editor_property(k,v)
 # 目标销毁：树进入等待并释放动作订阅；统一重置重新分配新目标。
 clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05)
 ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from until(p2.is_attacking,.3)
 p1.destroy_actor();yield from wait(.3)
 check('M5-T06_target_destroyed',ai.get_action_binding_count()==0 and ai.get_current_branch()==unreal.AIBranch.WAIT)
 gm.reset_training();p1=gm.get_player_fighter();yield from wait(.3)
 check('M5-T06_target_rebind',ai.get_component_by_class(unreal.BlackboardComponent).get_value_as_object('Target')==p1)
 # 反应只观察已起手攻击；对比延迟，使用远处起手避免被击中打断样本。
 for delay in [1.,0.]:
  clean();place(p1,210);place(p2,500,yaw=180)
  gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai()
  params(ai,decision_interval=.05,reaction_delay=delay,min_branch_hold_time=0.,attack_chance=0.,dodge_chance=0.,defend_chance=1.)
  inp(p1).notify_attack_pressed();yield from wait(.12)
  check('M5-T08_no_input_read_'+str(delay),not any('Branch=Defend' in x for x in ai.get_decision_log()))
  inp(p1).invalidate_session(unreal.Text('test release'));inp(p1).submit_light_attack();yield from wait(.3)
  defended=any('Branch=Defend' in x for x in ai.get_decision_log())
  check('M5-T08_reaction_'+str(delay),defended==(delay==0.),list(ai.get_decision_log()))
 # 近距离后退、侧移及边界失败均有明确分支/等待，不产生场外目的地。
 for branch in [unreal.AIBranch.RETREAT,unreal.AIBranch.STRAFE]:
  clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05)
  start=loc(p2);ai.debug_request_branch(branch);yield from wait(.25)
  check('M5-T02_'+str(branch),(loc(p2)-start).length()>2 and ai.is_legal_destination(loc(p2)),str(loc(p2)))
 clean();place(p1,800);place(p2,1040,yaw=180)
 gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05)
 ai.debug_request_branch(unreal.AIBranch.RETREAT);yield from wait(.3)
 check('M5-T02_wall_fallback',math.hypot(loc(p2).x,loc(p2).y)<=1100 and any('MoveRequested' in x or 'NavigationRejected' in x for x in ai.get_decision_log()))
 # 战斗位移控制权：真实重拳击退、重踢倒地、条件投技。
 for action in ['heavy_punch','heavy_kick']:
  clean();temp(p2.character_movement,'max_walk_speed',10.);place(p1,400);place(p2,500,yaw=180);yield from wait(.1)
  gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05,attack_chance=0.,retreat_distance=0.,dodge_chance=0.,defend_chance=0.)
  getattr(inp(p1),'submit_'+action)();peak=0.;end=now()+.8
  while now()<end:
   peak=max(peak,p2.get_velocity().x);yield
  check('M5-T05_'+action,hp(p2)<1000 and ('IDLE' in str(ai.get_move_status())),dict(hp=hp(p2),peak=peak,status=str(ai.get_move_status())))
  if action=='heavy_punch':check('M5-T05_knockback_preserved',peak>50,peak)
  else:check('M5-T05_knockdown',tag(p2,'State.KnockedDown'))
  yield from wait(2.5)
  check('M5-T05_resume_'+action,ai.is_ai_active() and p2.can_act())
  p2.character_movement.set_editor_property('max_walk_speed',500.)
 clean();place(p1,350);place(p2,500,yaw=180);yield from wait(.1)
 inp(p1).notify_guard_pressed();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05,attack_chance=0.,heavy_chance=0.,retreat_distance=0.,random_seed=12)
 ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from until(p2.is_throw_paired,1.)
 check('M5-T05_condition_throw',p1.is_throw_paired() and p2.is_throw_paired())
 check('M5-T05_throw_path_idle','IDLE' in str(ai.get_move_status()))
 inp(p1).notify_guard_released();yield from wait(1.5)
 check('M5-T05_throw_release',not p1.is_throw_paired() and not p2.is_throw_paired() and ai.get_action_binding_count()==0)
 # 同一场景分别由玩家入口与 BT 请求轻拳，扣血及资源结果相同。
 clean();place(p1,400);place(p2,500,yaw=180);yield from wait(.1)
 inp(p1).submit_light_attack();yield from wait(.6);player_damage=1000-hp(p2)
 clean();place(p1,400);place(p2,500,yaw=180);yield from wait(.1)
 gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();params(ai,decision_interval=.05,attack_chance=0.,heavy_chance=0.,retreat_distance=0.,random_seed=12)
 ai.debug_request_branch(unreal.AIBranch.ATTACK);yield from wait(.6)
 check('M5-T07_equal_melee_damage',player_damage==35 and 1000-hp(p1)==player_damage,dict(player=player_damage,ai=1000-hp(p1)))
 # 对局结果分别断言；菜单不能复活请求入口；三轮统一重置。
 for kind,expected in [('player',unreal.MatchOutcome.PLAYER_WIN),('opponent',unreal.MatchOutcome.OPPONENT_WIN),('draw',unreal.MatchOutcome.DRAW)]:
  clean();gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();count=gm.get_match_resolution_count()
  if kind in ('opponent','draw'):p1.jjk_debug_kill()
  if kind in ('player','draw'):p2.jjk_debug_kill()
  yield from until(gm.is_match_resolved,2)
  check('M5-T09_'+kind,gm.get_match_outcome()==expected,str(gm.get_match_outcome()))
  yield from wait(.3);check('M5-T09_once_'+kind,gm.get_match_resolution_count()==count+1 and not ai.is_ai_active() and ai.get_action_binding_count()==0)
  pc.set_training_panel_open(True);pc.set_training_panel_open(False)
  check('M5-T09_frozen_'+kind,inp(p1).submit_light_attack()==unreal.ActionRequestResult.REJECTED_BLOCKED and inp(p2).submit_light_attack()==unreal.ActionRequestResult.REJECTED_BLOCKED)
  gm.restart_match();yield from wait(.7)
  check('M5-T10_restart_'+kind,not gm.is_match_resolved() and not p1.is_dead() and not p2.is_dead() and ai.is_ai_active())
 clean()
gen=suite();started=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 for o,k,v in reversed(original):o.set_editor_property(k,v)
 clean();perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
 state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>240:raise TimeoutError('M5 timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
