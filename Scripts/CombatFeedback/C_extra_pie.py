"""Extra real-shot geometry, direction, concurrent ownership and screenshot checks.

Launched by `python Scripts/CombatFeedback/C_run.py extra`; all bindings are
temporary and no gameplay asset is saved. Screenshots: Saved/FeedbackC/*.png.
"""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0)
gm=pc.get_training_game_mode();p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fp,fq=p.get_combat_feedback(),q.get_combat_feedback()
vp=p.get_component_by_class(unreal.CombatRangedVisualConsumer)
vq=q.get_component_by_class(unreal.CombatRangedVisualConsumer)
out=Path(unreal.Paths.project_saved_dir())/'FeedbackC'
report={'status':'running','checks':[],'contacts':[],'screenshots':[]}
def write():(out/'extra-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,d=None):report['checks'].append({'name':n,'passed':bool(b),'detail':d});write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s)
def dist(a,b):return math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)
def reset(axis='x',gap=600):
    gm.reset_training();pc.set_combat_input_enabled(True)
    positions=[(p,0,0,0),(q,gap,0,180)] if axis=='x' else [(p,0,0,90),(q,0,gap,270)]
    for a,x,y,yaw in positions:
        a.character_movement.stop_movement_immediately()
        a.set_actor_location(unreal.Vector(x,y,100),False,True)
        a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def stance(a):a.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
def snap(name):
    path=out/(name+'.png')
    unreal.GameplayStatics.set_game_paused(w,True)
    cmd('HighResShot 1280x720 filename="'+path.as_posix()+'"')
    deadline=time.monotonic()+15
    while not path.exists() and time.monotonic()<deadline:yield
    ok=path.exists();report['screenshots'].append(str(path) if ok else None)
    check(name+' screenshot captured',ok)
    unreal.GameplayStatics.set_game_paused(w,False)
def shot(label,axis='x',gap=600,fps=60,screenshot=False):
    reset(axis,gap);yield from wait(.7);yield from stance(q)
    cmd('t.MaxFPS '+str(fps));yield from wait(.15)
    before=fq.contact_count;old=vq.spawned_cue_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>before,3)
    c=fq.last_contact
    row={'case':label,'contact_delta':fq.contact_count-before,'result':str(c.result),
         'cue_delta':vq.spawned_cue_count-old,'contact_location':str(c.location),
         'cue_location':str(vq.last_cue_location),'direction':str(c.direction),
         'cue_rotation':str(vq.last_cue_rotation)}
    report['contacts'].append(row);write()
    check(label+' true point and one cue',row['contact_delta']==1 and c.result==unreal.CombatFeedbackResult.HIT
          and row['cue_delta']==1 and dist(c.location,vq.last_cue_location)<1.,row)
    expected=math.degrees(math.atan2(-c.direction.y,-c.direction.x))
    delta=(vq.last_cue_rotation.yaw-expected+180)%360-180
    check(label+' cue faces against incoming direction',abs(delta)<2,{'expected_yaw':expected,'actual_yaw':vq.last_cue_rotation.yaw})
    if screenshot:yield from snap('C-'+label.replace(' ','-'))
    cmd('t.MaxFPS 60')

def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.RangedFX 1')
    yield from shot('near-X',gap=350,screenshot=True)
    yield from shot('far-X',gap=900)
    yield from shot('side-Y',axis='y',gap=500,screenshot=True)
    yield from shot('cross-frame-15fps',gap=600,fps=15)
    # Clean while a contact cue is still active, then verify owner-bound teardown.
    reset();yield from wait(.7);yield from stance(q)
    old=fq.contact_count;q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>old)
    had=vq.active_cue_count
    gm.reset_training();yield from wait(.05)
    check('C-T05 reset during active cue destroys it',had>0 and vq.active_cue_count==0,{'before':had,'after':vq.active_cue_count})
    # Both actors own independent visual arrays; clearing a new round clears both.
    reset();yield from wait(.7);yield from stance(p);yield from stance(q)
    oldp,oldq=vp.spawned_cue_count,vq.spawned_cue_count
    p.get_combat_input().notify_attack_pressed();q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    p.get_combat_input().notify_attack_released();q.get_combat_input().notify_attack_released()
    yield from until(lambda:vp.spawned_cue_count>oldp and vq.spawned_cue_count>oldq,3)
    check('C-T04 two source owners spawn separate contact cues',vp.spawned_cue_count>oldp and vq.spawned_cue_count>oldq,
          {'p':vp.spawned_cue_count-oldp,'q':vq.spawned_cue_count-oldq})
    gm.reset_training();yield from wait(.1)
    check('C-T05 both owner arrays retire on reset',vp.active_cue_count==0 and vq.active_cue_count==0)

    # A keeps already-fired projectiles alive across input/menu cleanup.
    reset();yield from wait(.7);yield from stance(q)
    p.set_actor_location(unreal.Vector(0,0,5000),False,True)
    q.set_actor_location(unreal.Vector(600,0,5000),False,True)
    p.character_movement.set_component_tick_enabled(False)
    q.character_movement.set_component_tick_enabled(False)
    old_fire=fq.fire_count;old_contact=fq.contact_count;old_life=fq.lifecycle_count;old_cue=vq.spawned_cue_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.fire_count>old_fire)
    gm.set_training_menu_open(True)
    p.set_actor_location(unreal.Vector(0,1000,100),False,True)
    yield from until(lambda:fq.lifecycle_count>old_life and fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE,5)
    check('C-T05 menu-cleared input keeps the fired expiry cue',fq.fire_count==old_fire+1 and
          fq.contact_count==old_contact and fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE and
          vq.spawned_cue_count==old_cue+1 and str(vq.last_cue_effect)=='PS_C_Expire_Laser',
          {'reason':str(fq.last_lifecycle.reason),'cue_delta':vq.spawned_cue_count-old_cue})
    gm.set_training_menu_open(False)
    p.character_movement.set_component_tick_enabled(True)
    q.character_movement.set_component_tick_enabled(True)
    yield from wait(.3)
    check('C-T05 menu expiry cue retires',vq.active_cue_count==0)

gen=suite();start=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.GameplayStatics.set_game_paused(w,False)
    gm.set_training_menu_open(False)
    p.character_movement.set_component_tick_enabled(True)
    q.character_movement.set_component_tick_enabled(True)
    gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-start;write()
def tick(dt):
    try:
        if time.monotonic()-start>120:raise TimeoutError()
        next(gen)
    except StopIteration:report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
