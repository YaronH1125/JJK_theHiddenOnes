"""PIE spacing regression: distance, angle, segment reacquisition and bounded windup."""
import json,time,traceback,math
from pathlib import Path
import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w
pc=unreal.GameplayStatics.get_player_controller(w,0)
gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fd=p.get_definition();inp=p.get_combat_input();hit=p.get_combat_hit()
out=Path(unreal.Paths.project_saved_dir())/'IG21/acceptance.json'
r={'status':'running','checks':[]}
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
old_autoface=fd.melee_auto_face
def write():out.write_text(json.dumps(r,indent=2),encoding='utf-8')
def check(name,ok,detail=None):r['checks'].append(dict(name=name,passed=bool(ok),detail=detail));write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
 end=now()+t
 while now()<end:yield
def until(pred,t=2):
 end=now()+t
 while not pred() and now()<end:yield
def hp():return q.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value')
def dist():return (q.get_actor_location()-p.get_actor_location()).length()
def reset(gap=220,angle=0,z=100):
 gm.reset_training();pc.set_combat_input_enabled(True)
 a=math.radians(angle)
 for f,pos,yaw in [(p,unreal.Vector(0,0,100),0),(q,unreal.Vector(gap*math.cos(a),gap*math.sin(a),z),angle+180)]:
  f.character_movement.stop_movement_immediately();f.set_actor_location(pos,False,True);f.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def da(n):return unreal.load_asset('/Game/Training/DA_M3_'+n)

def suite():
 yield from wait(.3)
 for fps in [30,60,120]:
  unreal.SystemLibrary.execute_console_command(w,'t.MaxFPS '+str(fps))
  for gap in [100,120,160,180,220,280,350]:
   reset(gap);yield from wait(.15)
   health=hp();before=p.get_actor_location();camera=pc.get_control_rotation()
   inp.submit_light_attack();yield from wait(.8)
   travel=(p.get_actor_location()-before).length()
   check(f'{fps}fps A1 at {gap}cm hits once',health-hp()==35,dict(damage=health-hp(),distance=dist(),travel=travel))
   check(f'{fps}fps {gap}cm bounded and camera unchanged',travel<=251 and abs(pc.get_control_rotation().yaw-camera.yaw)<.1)
 unreal.SystemLibrary.execute_console_command(w,'t.MaxFPS 60')
 for angle in [-80,-45,45,80]:
  reset(280,angle);yield from wait(.15)
  health=hp();inp.submit_light_attack();yield from wait(.8)
  check(f'front angle {angle} hits',health-hp()==35,dict(damage=health-hp(),distance=dist()))
 for label,gap,angle in [('outside acquisition',351,0),('behind target',220,180)]:
  reset(gap,angle);yield from wait(.15)
  before=p.get_actor_location();health=hp();inp.submit_light_attack();yield from wait(.8)
  check(label+' no movement or free damage',(p.get_actor_location()-before).length()<1 and hp()==health)
 reset(220);yield from wait(.2)
 fd.set_editor_property('melee_auto_face',False)
 before=p.get_actor_location();health=hp();inp.submit_light_attack();yield from wait(.8)
 check('disabled magnetism stays disabled',(p.get_actor_location()-before).length()<1 and hp()==health)
 fd.set_editor_property('melee_auto_face',old_autoface)

 # Walk P2 sideways during the windup, then hold still during the strike.
 reset(220);yield from wait(.15)
 health=hp();inp.submit_light_attack()
 for _ in wait(.11):
  q.set_actor_location(q.get_actor_location()+unreal.Vector(0,150*unreal.GameplayStatics.get_world_delta_seconds(w),0),False,True)
  yield
 yield from wait(.7)
 check('lateral windup movement still contacts',health-hp()==35,dist())

 # Target exits after hit window opens: no continued chase or magnetic damage.
 reset(220);yield from wait(.15)
 inp.submit_light_attack();yield from until(lambda:hit.is_window_open())
 before=p.get_actor_location();q.set_actor_location(unreal.Vector(600,200,100),False,True)
 yield from wait(.5)
 check('no chasing after active window begins',(p.get_actor_location()-before).length()<1)

 # Every combo segment reacquires; moving the target between segments must not
 # leave later attacks stuck facing the first segment's old position.
 reset(280);yield from wait(.15)
 health=hp();inp.submit_light_attack()
 for expected,name in [(1,'A1'),(2,'A2'),(6,'A3')]:
  yield from until(lambda:hit.get_segment_elapsed_time()>da(name).window_end_time+.015)
  pos=p.get_actor_location()+p.get_actor_forward_vector()*185
  q.set_actor_location(pos,False,True)
  inp.submit_light_attack();yield from until(lambda:hit.get_segment_id()==expected)
 yield from wait(1.3)
 check('all four punches reacquire and hit',health-hp()==185,health-hp())

 reset(280);yield from wait(.15)
 health=hp();inp.submit_kick()
 for expected,name in [(7,'Kick'),(8,'Kick2')]:
  yield from until(lambda:hit.get_segment_elapsed_time()>da(name).window_end_time+.025)
  q.set_actor_location(p.get_actor_location()+p.get_actor_forward_vector()*220,False,True)
  inp.submit_kick();yield from until(lambda:hit.get_segment_id()==expected)
 yield from wait(1.3)
 check('all three kicks reacquire and hit',health-hp()==135,health-hp())

 for label,press,release,damage in [('punch','notify_attack_pressed','notify_attack_released',90),('kick','notify_kick_pressed','notify_kick_released',100)]:
  reset(280);yield from wait(.15)
  health=hp();before=p.get_actor_location();getattr(inp,press)();yield from wait(1)
  check(label+' holding does not slide',(p.get_actor_location()-before).length()<1 and hp()==health)
  q.set_actor_location(unreal.Vector(330,35,100),False,True)
  getattr(inp,release)();yield from wait(1.3)
  check(label+' release reacquires moved target',health-hp()==damage,dict(damage=health-hp(),distance=dist()))

 reset(280);yield from wait(.15)
 inp.submit_light_attack();p.jjk_debug_force_hit_react()
 before=p.get_actor_location();yield from wait(.8)
 check('interruption stops approach',(p.get_actor_location()-before).length()<1)

gen=suite();started=time.monotonic()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 fd.set_editor_property('melee_auto_face',old_autoface)
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
 unreal.SystemLibrary.execute_console_command(w,'t.MaxFPS 0')
 gm.reset_training();pc.set_combat_input_enabled(True)
 r['seconds']=time.monotonic()-started;write()
def tick(dt):
 try:
  if time.monotonic()-started>160:raise TimeoutError()
  next(gen)
 except StopIteration:r['status']='passed' if all(c['passed'] for c in r['checks']) else 'failed';finish()
 except Exception:r['status']='failed';r['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
