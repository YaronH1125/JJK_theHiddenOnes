"""C visual consumer validation from real PIE attack/settlement paths.

Launched by C_run.py in a fresh PIE world. No synthetic contact events, asset
saves, or gameplay value changes. Checks use A's actual Contact/Lifecycle.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(w, 0)
gm = pc.get_training_game_mode()
p, q = gm.get_player_fighter(), gm.get_opponent_fighter()
fd = p.get_definition()
old_initial_energy = fd.initial_energy
fp, fq = p.get_combat_feedback(), q.get_combat_feedback()
vp = p.get_component_by_class(unreal.CombatRangedVisualConsumer)
vq = q.get_component_by_class(unreal.CombatRangedVisualConsumer)
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC/pie-report.json'
report = {'status':'running', 'engine':unreal.SystemLibrary.get_engine_version(), 'checks':[],
          'cases':[], 'test_binding':'transient C_setup only; A final assignment pending'}

def write(): out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
def check(name, ok, detail=None):
    report['checks'].append({'name':name, 'passed':bool(ok), 'detail':detail})
    write()
def now(): return unreal.GameplayStatics.get_time_seconds(w)
def wait(seconds):
    end = now() + seconds
    while now() < end: yield
def until(pred, seconds=3):
    end = now() + seconds
    while not pred() and now() < end: yield
def cmd(command): unreal.SystemLibrary.execute_console_command(w, command)
def hp(f): return f.get_fighter_attribute_set().health.current_value
def reset(gap=600):
    gm.reset_training();pc.set_combat_input_enabled(True)
    for actor, x, yaw in [(p,0,0),(q,gap,180)]:
        actor.character_movement.stop_movement_immediately()
        actor.set_actor_location(unreal.Vector(x,0,100),False,True)
        actor.set_actor_rotation(unreal.Rotator(0,yaw,0),False)
def stance(f):
    f.get_combat_input().notify_stance_switch_pressed()
    yield from wait(.6)
def distance(a,b): return ((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)**.5
def find_wall(x=300):
    old = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)}
    gm.jjk_spawn_blocker(float(x),0.,.3,6.,5.)
    for actor in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if actor.get_name() not in old and actor.get_component_by_class(unreal.StaticMeshComponent):
            actor.set_actor_location(unreal.Vector(x,0,250),False,True)
def fired_case(label, case, charge=.4):
    reset();yield from wait(.7);yield from stance(q)
    if case == 'guard': p.get_combat_input().notify_guard_pressed()
    if case == 'expire':
        p.set_actor_location(unreal.Vector(0,0,5000),False,True)
        q.set_actor_location(unreal.Vector(600,0,5000),False,True)
        p.character_movement.set_component_tick_enabled(False)
        q.character_movement.set_component_tick_enabled(False)
    old_contact=fq.contact_count;old_life=fq.lifecycle_count;old_fire=fq.fire_count
    old_cues=vq.spawned_cue_count;before=hp(p)
    q.get_combat_input().notify_attack_pressed();yield from wait(charge)
    if case=='wall': find_wall()
    q.get_combat_input().notify_attack_released()
    if case=='immune':
        yield from wait(.1)
        p.request_dodge(unreal.Vector(0,1,0))
        p.character_movement.set_component_tick_enabled(False)
    if case=='expire':
        yield from until(lambda:fq.fire_count>old_fire)
        p.set_actor_location(unreal.Vector(0,1000,100),False,True)
    if case=='expire': yield from until(lambda:fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE and fq.lifecycle_count>old_life,5)
    else: yield from until(lambda:fq.contact_count>old_contact,3)
    contact=fq.last_contact
    row={'case':label,'fire_delta':fq.fire_count-old_fire,'contact_delta':fq.contact_count-old_contact,
         'result':str(contact.result) if case!='expire' else None,'damage':before-hp(p),
         'end_reason':str(fq.last_lifecycle.reason),
         'cue_delta':vq.spawned_cue_count-old_cues,'active_cues':vq.active_cue_count,
         'cue_effect':str(vq.last_cue_effect),'cue_location':str(vq.last_cue_location),
         'contact_location':str(contact.location) if case!='expire' else None}
    report['cases'].append(row);write()
    check(label+' uses one real Fire',row['fire_delta']==1,row)
    if case=='expire':
        check(label+' has no contact and only small expiry cue',row['contact_delta']==0 and
              fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE and row['cue_delta']==1 and
              str(vq.last_cue_effect)=='PS_C_Expire_Laser' and row['damage']==0,row)
    else:
        result={'hit':unreal.CombatFeedbackResult.HIT,'guard':unreal.CombatFeedbackResult.GUARD,
                'immune':unreal.CombatFeedbackResult.IMMUNE,'wall':unreal.CombatFeedbackResult.WORLD_IMPACT}[case]
        effect={'hit':'PS_C_Hit_Frost','guard':'PS_C_Guard_Laser','immune':'PS_C_Immune_Laser',
                'wall':'PS_C_World_Sand'}[case]
        check(label+' chooses settled result and effect',row['contact_delta']==1 and contact.result==result and
              row['cue_delta']==1 and str(vq.last_cue_effect)==effect,row)
        check(label+' spawns at real contact point',distance(vq.last_cue_location,contact.location)<1.,row)
        if case in ('immune','wall'): check(label+' does not damage victim',row['damage']==0,row)
        if case=='hit': check(label+' keeps real damage',row['damage']>0,row)
    p.character_movement.set_component_tick_enabled(True)
    q.character_movement.set_component_tick_enabled(True)
    p.get_combat_input().notify_guard_released()
    gm.jjk_clear_blockers()
    cue = vq.visual_profile.get_editor_property({'hit':'mobile_hit','guard':'guard','immune':'immune','wall':'world_impact','expire':'expire'}[case])
    yield from wait(cue.max_life+cue.fade_life+.08)
    check(label+' cue lifetime ends',vq.active_cue_count==0,{'active':vq.active_cue_count})

def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.RangedFX 1')
    check('Both source-owned consumers exist with hard profile',bool(vp and vq and vp.visual_profile and vq.visual_profile))
    check('Transient old impact is disabled',bool(p.get_definition().feedback_profile.external_ranged_impact and
          q.get_definition().feedback_profile.external_ranged_impact))
    for case in ['hit','guard','immune','wall','expire']:
        yield from fired_case('C-T01 '+case,case)

    reset();yield from wait(.7);yield from stance(q)
    before=hp(p);contact=fq.contact_count;old=vq.spawned_cue_count;fire=fq.fire_count
    q.get_combat_input().notify_kick_pressed();yield from wait(3.3)
    q.get_combat_input().notify_kick_released()
    yield from until(lambda:fq.contact_count>contact,4)
    c=fq.last_contact
    check('C-T04 real Super uses paid strength and short Frost cue',fq.fire_count==fire+1 and
          c.tier==unreal.CombatFeedbackTier.SUPER_BLAST and c.paid_q>0.99 and
          hp(p)<before and vq.spawned_cue_count==old+1 and str(vq.last_cue_effect)=='PS_C_Hit_Frost' and
          distance(vq.last_cue_location,c.location)<1.,{'paid_q':c.paid_q,'damage':before-hp(p)})
    cue = vq.visual_profile.super_hit
    yield from wait(cue.max_life+cue.fade_life+.08)
    check('C-T04 Super natural tail is retired within configured bound',vq.active_cue_count==0,vq.active_cue_count)

    fd.set_editor_property('initial_energy',100.)
    reset();yield from wait(.7)
    old_fire=fp.fire_count;old_contact=fp.contact_count;old_cues=vp.spawned_cue_count
    p.get_combat_input().notify_domain_pressed()
    yield from until(lambda:fp.fire_count>old_fire,3)
    check('C-T04 curved domain orb gains bounded breadcrumb trail',fp.fire_count>old_fire and
          vp.active_orb_trail_count>=1,{'fire':fp.fire_count-old_fire,'trail':vp.active_orb_trail_count})
    yield from until(lambda:fp.contact_count>old_contact,3)
    c=fp.last_contact
    check('C-T04 domain cue uses real contact and clears trail',c.tier==unreal.CombatFeedbackTier.DOMAIN_ORB and
          c.result==unreal.CombatFeedbackResult.HIT and vp.spawned_cue_count>old_cues and
          distance(vp.last_cue_location,c.location)<1. and vp.active_orb_trail_count==0,
          {'result':str(c.result),'cue_delta':vp.spawned_cue_count-old_cues,'trail':vp.active_orb_trail_count})
    gm.reset_training();yield from wait(.2)
    fd.set_editor_property('initial_energy',old_initial_energy)
    check('C-T05 reset clears both owners without lingering instances',vp.active_cue_count==0 and
          vq.active_cue_count==0 and vp.active_orb_trail_count==0 and vq.active_orb_trail_count==0)

    reset();yield from wait(.7);yield from stance(q)
    old=vq.spawned_cue_count;before=hp(p)
    cmd('JJK.Feedback.RangedFX 0')
    q.get_combat_input().notify_attack_pressed();yield from wait(.4);q.get_combat_input().notify_attack_released()
    yield from until(lambda:hp(p)<before,3)
    check('C-T07 RangedFX off keeps settlement and spawns no cue',hp(p)<before and vq.spawned_cue_count==old,
          {'damage':before-hp(p),'cue_delta':vq.spawned_cue_count-old})
    cmd('JJK.Feedback.RangedFX 1')

    reset();yield from wait(.7);yield from stance(q)
    old=vq.spawned_cue_count
    q.get_combat_input().notify_attack_pressed();yield from wait(60)
    check('C-T06 60 s hold creates no contact cue',vq.spawned_cue_count==old and vq.active_cue_count==0)
    q.get_combat_input().notify_attack_released();yield from wait(1)
    gm.reset_training();yield from wait(.2)
    check('C-T06 60 s hold plus reset leaves no visual component',vp.active_cue_count==0 and vq.active_cue_count==0)

    for i in range(20):
        reset();yield from wait(.025)
    check('C-T05 twenty mixed-round resets remain clean',vp.active_cue_count==0 and vq.active_cue_count==0 and
          vp.active_orb_trail_count==0 and vq.active_orb_trail_count==0)

gen=suite();start=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    p.character_movement.set_component_tick_enabled(True)
    q.character_movement.set_component_tick_enabled(True)
    gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    fd.set_editor_property('initial_energy',old_initial_energy)
    report['seconds']=time.monotonic()-start
    write()
def tick(dt):
    try:
        if time.monotonic()-start>220: raise TimeoutError('C PIE callback')
        next(gen)
    except StopIteration:
        report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed';finish()
    except Exception:
        report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
