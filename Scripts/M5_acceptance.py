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
