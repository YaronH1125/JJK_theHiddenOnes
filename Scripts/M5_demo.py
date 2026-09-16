"""图形 PIE：真实 UMG 模式操作、AI 造成致死伤害、结果、重开与模式切换。"""
import unreal,time,json,traceback
from pathlib import Path
assert '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode();p1=gm.get_player_fighter();p2=gm.get_opponent_fighter();fd=p1.get_definition()
old_hp=fd.get_editor_property('initial_health')
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings');old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()));out=root/'Saved/M5_demo.json'
state={'status':'running','checks':[],'started_at':time.time()}
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,value,details=None):state['checks'].append(dict(name=name,passed=bool(value),details=details));write()
def panel():return pc.get_editor_property('training_panel')
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(fn,t):
 end=now()+t
 while now()<end and not fn():yield
def shot(name):
 path=root/'Docs/开发过程/验收记录'/name
 unreal.SystemLibrary.execute_console_command(world,'Shot showui filename="'+path.as_posix()+'" -nosuffix')
 return path
def suite():
 unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
 gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.set_training_settings(unreal.TrainingSettings())
 fd.set_editor_property('initial_health',70.);gm.reset_training()
 pc.set_control_rotation(unreal.Rotator(-12,-35,0))
 pc.set_training_panel_open(True);yield from wait(.2)
 check('M5-demo_AI_button_enabled',not panel().is_ai_option_disabled())
 panel().call_method('AIClicked');yield from wait(.2);ai=gm.get_opponent_ai()
 check('M5-demo_selected_AI_menu_paused',gm.get_editor_property('opponent_mode')==unreal.OpponentMode.AI and not ai.is_ai_active())
 panel().call_method('CloseClicked');yield from wait(.3)
 check('M5-demo_close_starts_tree',ai.is_ai_active())
 p=ai.get_params();p.attack_chance=1.;p.heavy_chance=0.;p.retreat_distance=0.;p.random_seed=12;ai.set_params(p)
 yield from until(lambda:(p1.get_actor_location()-p2.get_actor_location()).length()<300,10)
 check('M5-demo_approach',(p1.get_actor_location()-p2.get_actor_location()).length()<300)
 gm.call_method('JJKDebugHud');fight=shot('M5_AI对战.png');yield from wait(.8);gm.call_method('JJKDebugHud')
 check('M5-demo_fight_capture',fight.exists() and fight.stat().st_mtime>=state['started_at'])
 yield from until(gm.is_match_resolved,18)
 check('M5-demo_natural_GAS_defeat',p1.is_dead() and gm.get_match_outcome()==unreal.MatchOutcome.OPPONENT_WIN)
 check('M5-demo_result_UMG',gm.is_training_menu_open() and '对手胜利' in panel().get_displayed_state(),panel().get_displayed_state())
 result=shot('M5_胜负与重开.png');yield from wait(.8)
 check('M5-demo_result_capture',result.exists() and result.stat().st_mtime>=state['started_at'])
 panel().call_method('ResetClicked');yield from wait(.2)
 check('M5-demo_reset_menu_keeps_pause',not gm.is_match_resolved() and not p1.is_dead() and not ai.is_ai_active())
 panel().call_method('CloseClicked');yield from wait(.3)
 check('M5-demo_restart_AI',ai.is_ai_active())
 pc.set_training_panel_open(True);panel().get_editor_property('OpponentChoice').set_selected_option('静止木桩');panel().call_method('CloseClicked');yield from wait(.2)
 check('M5-demo_static_stop',p2.get_controller() is None and not p2.is_guard_intent())
 pc.set_training_panel_open(True);panel().call_method('AIClicked');panel().call_method('CloseClicked');yield from wait(.3)
 check('M5-demo_AI_again',gm.get_opponent_ai().is_ai_active())
gen=suite();started=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle);fd.set_editor_property('initial_health',old_hp)
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.reset_training()
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle);state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>180:raise TimeoutError('M5 demo timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if len(state['checks'])==12 and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
