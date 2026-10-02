"""Measure stationary P2 reach under the real magnetism and montage."""
import json,time,traceback
from pathlib import Path
import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0)
gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fd=p.get_definition()
out=Path(unreal.Paths.project_saved_dir())/'IG21'
out.mkdir(exist_ok=True)
r={'status':'running','settings':{k:str(fd.get_editor_property(k)) for k in ['melee_auto_face','attack_reach','magnetism_range','magnetism_lunge']},'cases':[]}
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
 end=now()+t
 while now()<end:yield
def hp():return q.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value')
def dist():return (q.get_actor_location()-p.get_actor_location()).length()
def suite():
 yield from wait(.3)
 for gap in [100,120,140,160,180,220,280,350]:
  gm.reset_training();pc.set_combat_input_enabled(True)
  for f,x,yaw in [(p,0,0),(q,gap,180)]:
   f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,0,100),False,True);f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
  yield from wait(.2)
  before=dist();health=hp();p.get_combat_input().submit_light_attack();after=dist()
  yield from wait(.8)
  r['cases'].append(dict(gap=gap,before=before,after=after,damage=health-hp()))
 r['status']='done'
g=suite();start=time.monotonic()
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def tick(dt):
 try:
  if time.monotonic()-start>40:raise TimeoutError()
  next(g)
 except (StopIteration,Exception) as e:
  if not isinstance(e,StopIteration):r['status']='error';r['error']=traceback.format_exc()
  unreal.unregister_slate_post_tick_callback(h);perf.set_editor_property('bThrottleCPUWhenNotForeground',old);gm.reset_training();pc.set_combat_input_enabled(True)
 (out/'before.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
h=unreal.register_slate_post_tick_callback(tick)
