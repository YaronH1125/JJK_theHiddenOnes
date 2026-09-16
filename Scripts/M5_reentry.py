"""新 PIE 的模式/目标/导航/重置验证；仅验收，不修资产。"""
import unreal,json,time,traceback
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
gm=unreal.GameplayStatics.get_game_mode(w);p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
out=Path(unreal.Paths.project_saved_dir())/'M5_reentry.json'
state={'status':'running','checks':[]}
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,b):state['checks'].append(dict(name=name,passed=bool(b)))
def wait(t):
 end=unreal.GameplayStatics.get_time_seconds(w)+t
 while unreal.GameplayStatics.get_time_seconds(w)<end:yield
def suite():
 check('default_static',gm.get_editor_property('opponent_mode')==unreal.OpponentMode.STATIC and p2.get_controller() is None)
 gm.set_opponent_mode(unreal.OpponentMode.AI);ai=gm.get_opponent_ai();yield from wait(2)
 check('running_tree',ai.is_ai_active())
 check('target',ai.get_component_by_class(unreal.BlackboardComponent).get_value_as_object('Target')==p1)
 check('navigation',ai.is_navigation_ready())
 check('approach',(p1.get_actor_location()-p2.get_actor_location()).length()<950)
 gm.reset_training();yield from wait(.5)
 check('reset_tree',ai.is_ai_active())
 gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 check('stop_clean',p2.get_controller() is None and not p2.is_guard_intent())
gen=suite();started=time.monotonic()
def tick(dt):
 try:
  if time.monotonic()-started>45:raise TimeoutError('reentry timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if len(state['checks'])==7 and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
def finish():
 unreal.unregister_slate_post_tick_callback(handle);write()
write();handle=unreal.register_slate_post_tick_callback(tick)
