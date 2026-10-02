"""Capture real C consumer cues from a side-biased combat camera; no asset saves.

python Scripts/CombatFeedback/C_run.py visual
PNG and visual-report.json go to Saved/FeedbackC. The capture is a rendered
PIE observation, not a packaged-game or subjective player review.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0)
gm=pc.get_training_game_mode();p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fp=p.get_combat_feedback();fq=q.get_combat_feedback()
vp=p.get_component_by_class(unreal.CombatRangedVisualConsumer)
vq=q.get_component_by_class(unreal.CombatRangedVisualConsumer)
fd=p.get_definition();old_energy=fd.initial_energy
out=Path(unreal.Paths.project_saved_dir())/'FeedbackC'
run_id=time.strftime('%Y%m%d-%H%M%S')
report={'status':'running','checks':[],'files':[]}
def write():(out/'visual-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,d=None):report['checks'].append({'name':n,'passed':bool(b),'detail':d});write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s)
def reset():
    gm.reset_training();pc.set_combat_input_enabled(True)
    p.set_actor_location(unreal.Vector(0,0,100),False,True)
    q.set_actor_location(unreal.Vector(600,0,100),False,True)
    p.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
    q.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
def snap(name):
    path=out/(name+'-'+run_id+'.png')
    unreal.GameplayStatics.set_game_paused(w,True)
    cmd('HighResShot 1280x720 filename="'+path.as_posix()+'"')
    deadline=time.monotonic()+15
    while not path.exists() and time.monotonic()<deadline:yield
    report['files'].append(path.as_posix() if path.exists() else None)
    check(name+' rendered',path.exists())
    unreal.GameplayStatics.set_game_paused(w,False)
def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.RangedFX 1')
    reset();yield from wait(.6)
    pc.set_control_rotation(unreal.Rotator(pitch=-8,yaw=35,roll=0))
    yield from wait(.2);yield from snap('C-view-before')
    q.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
    old=fq.contact_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>old)
    yield from wait(.06)
    check('Mobile cue alive at capture',vq.active_cue_count>0)
    yield from snap('C-mobile-contact')
    yield from wait(.65);yield from snap('C-mobile-cleared')
    check('Mobile cue gone after its lifetime',vq.active_cue_count==0)

    reset();yield from wait(.6)
    q.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
    old=fq.contact_count
    q.get_combat_input().notify_kick_pressed();yield from wait(3.3)
    q.get_combat_input().notify_kick_released();yield from until(lambda:fq.contact_count>old,4)
    yield from wait(.06)
    check('Super cue alive at capture',vq.active_cue_count>0)
    yield from snap('C-super-contact')
    yield from wait(.65);yield from snap('C-super-cleared')
    check('Super cue gone by 0.54 seconds',vq.active_cue_count==0)

    fd.set_editor_property('initial_energy',100.)
    reset();yield from wait(.6)
    fire=fp.fire_count
    p.get_combat_input().notify_domain_pressed()
    yield from until(lambda:fp.fire_count>fire,3)
    yield from wait(.22)
    check('Domain visual trail alive during actual curved flight',vp.active_orb_trail_count>0)
    yield from snap('C-domain-curve')
    yield from until(lambda:vp.active_orb_trail_count==0,3)
    check('Domain visual trail removed at contact/end',vp.active_orb_trail_count==0)
    fd.set_editor_property('initial_energy',old_energy)

gen=suite();start=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.GameplayStatics.set_game_paused(w,False)
    gm.reset_training();pc.set_combat_input_enabled(True)
    fd.set_editor_property('initial_energy',old_energy)
    report['seconds']=time.monotonic()-start;write()
def tick(dt):
    try:
        if time.monotonic()-start>100:raise TimeoutError()
        next(gen)
    except StopIteration:report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
