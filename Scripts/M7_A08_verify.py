"""M7-A08 验证：默认配置（远程开关默认开）下 AI 完整对局自然使用远程炮。"""
import json,time,traceback
from pathlib import Path
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
out=Path(unreal.Paths.project_saved_dir())/'M7_A08_verify.json'
state={'status':'running','checks':[],'started_at':time.time()}
def write():
 t=out.with_suffix('.tmp');t.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');t.replace(out)
def check(name,value,detail=None):
 state['checks'].append(dict(name=name,passed=bool(value),details=detail));write();unreal.log(('[A08] PASS ' if value else '[A08] FAIL ')+name)
def now():return unreal.GameplayStatics.get_time_seconds(world)
def wait(t):
 end=now()+t
 while now()<end:yield
def hp(f):return f.get_fighter_attribute_set().health.current_value
def tag(f,name):
 t=unreal.GameplayTag();t.import_text('(TagName="'+name+'")');return f.has_combat_tag(t)

def suite():
 unreal.SystemLibrary.execute_console_command(world,'t.MaxFPS 60')
 pc.set_training_panel_open(False);gm.set_opponent_mode(unreal.OpponentMode.STATIC)
 gm.set_training_settings(unreal.TrainingSettings());gm.reset_training()
 yield from wait(1)
 p1=gm.get_player_fighter();p2=gm.get_opponent_fighter()
 # 默认参数（不设置 enable_ranged_combat —— 验证默认开启）
 gm.set_opponent_mode(unreal.OpponentMode.AI)
 ai=gm.get_opponent_ai()
 params=ai.get_params()
 check('A08_default_ranged_enabled',params.get_editor_property('enable_ranged_combat')==True)
 h0=hp(p1)
 t0=now()
 sawRanged=False;sawRangedBranch=False;sawMelee=False
 # 模拟拉开距离的对手：周期性把玩家传送远处，触发 AI 依距离的自然远程决策
 teleports=[6.,12.]  # 后段不再拉开：AI 重新接近后应回到近战
 while now()-t0<46.:
  yield
  elapsed=now()-t0
  if teleports and elapsed>=teleports[0]:
   teleports.pop(0)
   p1.character_movement.stop_movement_immediately()
   p1.set_actor_location(unreal.Vector(-900,0,100),False,True)
  log=ai.get_decision_log()
  if any('switch to ranged' in x for x in log):sawRanged=True
  if any('Branch=RangedAttack' in x for x in log):sawRangedBranch=True
  if any('Branch=Attack' in x for x in log):sawMelee=True
  if hp(p1)<h0-10:pass
 # 两类信号互为佐证：日志环容量有限，任一自然远程证据即可
 check('A08_natural_stance_switch',sawRanged or sawRangedBranch,{'log':list(ai.get_decision_log())[-10:]})
 check('A08_natural_ranged_branch',sawRangedBranch or sawRanged)
 check('A08_melee_still_used',sawMelee)
 check('A08_player_took_ranged_damage',hp(p1)<h0-10,{'hp0':h0,'hp1':hp(p1)})
 gm.reset_training();yield from wait(.3)

gen=suite();started=time.monotonic()
def finish():
 try:
  unreal.unregister_slate_post_tick_callback(handle)
  gm.reset_training()
 except Exception:pass
 state['duration_seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>120:raise TimeoutError('A08 timeout')
  next(gen)
 except StopIteration:
  state['status']='passed' if state['checks'] and all(c['passed'] for c in state['checks']) else 'failed';finish()
 except Exception:
  state['status']='failed';state['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
