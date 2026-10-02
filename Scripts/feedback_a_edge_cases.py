"""Feedback A edge cases, real sweeps plus separately labeled synthetic clock stress.

Run: python Scripts/run_feedback_a.py edge_cases
Reuses only the helper definitions of feedback_a_regression.py; no second callback.
All temporary assets/settings are restored. No imports or saves of game assets.
"""
from pathlib import Path
helper = Path(__file__).with_name('feedback_a_regression.py').read_text(encoding='utf-8')
exec(compile(helper.split('gen=suite();started=')[0], str(Path(__file__).with_name('feedback_a_regression.py')), 'exec'))
out=Path(unreal.Paths.project_saved_dir())/'FeedbackA/edge_cases.json'
r={'status':'running','checks':[],'fps':[]}

def wall(x,y,width):
    before={a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)}
    gm.jjk_spawn_blocker(x,y,width,6.,5.)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.get_name() not in before and a.get_component_by_class(unreal.StaticMeshComponent):
            a.set_actor_location(unreal.Vector(x,y,250),False,True)

def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 1')
    reset(120);yield from wait(.3)
    a,b=hp(p),hp(q);ca,cb=fp.contact_count,fq.contact_count
    inp.submit_light_attack();q.get_combat_input().submit_light_attack()
    yield from wait(1)
    check('T04 real same-frame mutual hits',hp(p)==a-35 and hp(q)==b-35 and fp.contact_count==ca+1 and fq.contact_count==cb+1,{'p_damage':a-hp(p),'q_damage':b-hp(q)})

    # Freeze a real montage, then inject stop requests to isolate cap/rate handling.
    reset(1000);yield from wait(.2)
    temp(p.mesh,'global_anim_rate_scale',.73)
    inp.submit_light_attack();yield from wait(.07)
    anim=p.mesh.get_anim_instance();m=da('A1').montage
    anim.montage_set_play_rate(m,1.37)
    fp.debug_request_stop(.065);yield from until(fp.is_stopped)
    started_stop=now();local=p.get_action_time();pos=anim.montage_get_position(m)
    frozen=True
    while fp.is_stopped():
        frozen &= abs(p.get_action_time()-local)<.0001 and abs(anim.montage_get_position(m)-pos)<.0001
        if now()-started_stop<.08:fp.debug_request_stop(.065)
        yield
    check('T04 synthetic merge retains first-start 100ms cap',frozen and .099<=fp.get_last_stop_duration()<=.1+unreal.GameplayStatics.get_world_delta_seconds(w)+.003,fp.get_last_stop_duration())
    check('T04 synthetic restores original montage/global rates',abs(anim.montage_get_play_rate(m)-1.37)<.001 and abs(p.mesh.global_anim_rate_scale-.73)<.001,{'rate':anim.montage_get_play_rate(m),'global':p.mesh.global_anim_rate_scale})
    p.mesh.set_editor_property('global_anim_rate_scale',1.)

    for legal in [True,False]:
        d=da('A1');old_end=d.cancel_window_end_time
        if not legal:temp(d,'cancel_window_end_time',.05)
        reset();yield from wait(.25);ca=fp.contact_count;inp.submit_light_attack()
        yield from until(lambda:fp.contact_count>ca)
        if legal:
            yield from until(lambda:hit.get_segment_elapsed_time()>d.window_end_time+.02)
            fp.debug_request_stop(.065);yield from until(fp.is_stopped)
        resource=p.get_action_resource();accepted=p.request_dodge(unreal.Vector(0,1,0))
        check('T03 '+('legal recovery (synthetic stop)' if legal else 'illegal contact')+' Shift during real stop',accepted==legal)
        yield from wait(.3)
        check('T03 Shift pays exactly once or remains rejected',abs(resource-p.get_action_resource()-(fd.dodge_config.cancel_dodge_total_cost if legal else 0))<.01)
        d.set_editor_property('cancel_window_end_time',old_end)

    # Real immune contact: stop the dodge movement component just for geometric overlap;
    # its actual invulnerability rule/GA runs normally and is not injected.
    reset(120);yield from wait(.3);ca=fp.contact_count;before=hp(q)
    q.request_dodge(unreal.Vector(0,1,0));q.character_movement.set_component_tick_enabled(False)
    inp.submit_light_attack();yield from until(lambda:fp.contact_count>ca,.24)
    check('T01 actual dodge rule produces Immune without freeze/damage',fp.contact_count==ca+1 and fp.last_contact.result==unreal.CombatFeedbackResult.IMMUNE and hp(q)==before and not fp.is_stopped() and not fq.is_stopped())
    q.character_movement.set_component_tick_enabled(True)

    # Armor is a temporary test configuration on the existing heavy attack.
    heavy=da('HeavyPunch');temp(heavy,'grants_super_armor',True)
    reset(120);yield from wait(.25);before=hp(q);ca=fp.contact_count
    q.get_combat_input().submit_heavy_punch();inp.submit_light_attack()
    yield from until(lambda:fp.contact_count>ca)
    check('T06 armored side takes damage but keeps montage and time',hp(q)==before-35 and fp.last_contact.armored and not fq.is_stopped() and q.is_attacking())
    heavy.set_editor_property('grants_super_armor',original[-1][2])

    old_hp=fd.initial_health;temp(fd,'initial_health',20.)
    reset();yield from wait(.25);ca=fp.contact_count;inp.submit_light_attack()
    yield from until(lambda:fp.contact_count>ca)
    check('T06 lethal prioritizes death and no stop',fp.last_contact.lethal and q.is_dead() and not fq.is_stopped() and not fp.is_stopped())
    fd.set_editor_property('initial_health',old_hp)
    settings=unreal.TrainingSettings();settings.set_editor_property('infinite_health',True)
    gm.set_training_settings(settings);temp(fd,'initial_health',1.)
    reset();yield from wait(.25);ca=fp.contact_count;inp.submit_light_attack()
    yield from until(lambda:fp.contact_count>ca)
    check('T06 infinite life Hit with zero actual damage',fp.last_contact.result==unreal.CombatFeedbackResult.HIT and fp.last_contact.actual_damage==0 and hp(q)==1 and not q.is_dead())
    fd.set_editor_property('initial_health',old_hp);gm.set_training_settings(unreal.TrainingSettings())

    # Own stun effect and timer must use the same local clock.
    reset();yield from wait(.25);ca=fp.contact_count;inp.submit_heavy_punch()
    yield from until(lambda:fp.contact_count>ca);start=q.get_action_time()
    yield from until(lambda:not tag(q,'State.HitStun'))
    elapsed=q.get_action_time()-start;step=unreal.GameplayStatics.get_world_delta_seconds(w)
    check('T05 GE and unlock retain configured local stun duration',abs(elapsed-heavy.hit_stun_duration)<=step+.005 and q.can_act(),{'local':elapsed,'configured':heavy.hit_stun_duration,'tick':step})

    # Ranged real geometry: AI uses current target position for the aim ray.
    for result in ['guard','immune','wall','expire','muzzle']:
        reset(600);yield from wait(.7);yield from ranged(q)
        ca=fq.contact_count;fire=fq.fire_count;life=fq.lifecycle_count;before=hp(p)
        if result=='expire':
            p.set_actor_location(unreal.Vector(0,0,5000),False,True);q.set_actor_location(unreal.Vector(600,0,5000),False,True)
            p.character_movement.set_component_tick_enabled(False);q.character_movement.set_component_tick_enabled(False)
        if result=='guard':inp.notify_guard_pressed()
        q.get_combat_input().notify_attack_pressed();yield from wait(.4)
        if result=='wall':wall(300.,0.,.3)
        if result=='muzzle':
            muzzle=q.mesh.get_socket_location(q.get_muzzle_socket_name()) if hasattr(q,'get_muzzle_socket_name') else q.mesh.get_socket_location(fd.muzzle_socket)
            # A thin wall immediately ahead of the head socket; refine from actual Fire absence.
            wall(muzzle.x-15,muzzle.y,.2)
        q.get_combat_input().notify_attack_released()
        if result=='immune':
            yield from wait(.1)
            p.request_dodge(unreal.Vector(0,1,0));p.character_movement.set_component_tick_enabled(False)
        if result=='expire':
            yield from until(lambda:fq.fire_count>fire)
            p.set_actor_location(unreal.Vector(0,1000,100),False,True)
        yield from wait(4 if result=='expire' else .6)
        if result=='muzzle':
            # Observe the actual recovery exit; .6 seconds is marginal at lower FPS.
            yield from until(lambda:fq.last_action.stage==unreal.CombatActionStage.END,2)
        if result=='guard':check('T07 real ranged Guard',fq.contact_count==ca+1 and fq.last_contact.result==unreal.CombatFeedbackResult.GUARD)
        if result=='immune':check('T07 real ranged Immune',fq.contact_count==ca+1 and fq.last_contact.result==unreal.CombatFeedbackResult.IMMUNE and hp(p)==before)
        if result=='wall':check('T08 real wall endpoint',fq.contact_count==ca+1 and fq.last_contact.result==unreal.CombatFeedbackResult.WORLD_IMPACT and abs(fq.last_contact.location.x-315)<2 and hp(p)==before,str(fq.last_contact.location))
        if result=='expire':check('T07 projectile expiry is lifecycle only',fq.contact_count==ca and fq.lifecycle_count>life and fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE,str(fq.last_lifecycle.reason))
        if result=='muzzle':check('T08 muzzle rejection has no successful Fire',fq.fire_count==fire and fq.last_action.end_reason==unreal.CombatFeedbackEnd.REJECTED,{'fire_delta':fq.fire_count-fire,'reason':str(fq.last_action.end_reason)})
        p.character_movement.set_component_tick_enabled(True);q.character_movement.set_component_tick_enabled(True);gm.jjk_clear_blockers()

    # Menu cleanup invalidates old feedback callbacks, not already-fired gameplay.
    reset(600);yield from wait(.7);yield from ranged(q)
    fire=fq.fire_count;ca=fq.contact_count;before=hp(p)
    q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.fire_count>fire)
    old_round=gm.get_feedback_round_id();old_gen=fq.get_generation()
    gm.set_training_menu_open(True)
    yield from until(lambda:fq.contact_count>ca,3)
    check('T07 menu cleanup preserves a real in-flight projectile',fq.fire_count==fire+1 and fq.contact_count==ca+1
          and hp(p)<before and fq.last_contact.result==unreal.CombatFeedbackResult.HIT
          and not fq.is_current(old_round,old_gen) and fq.last_contact.source_generation==fq.get_generation(),
          {'fire_delta':fq.fire_count-fire,'contacts':fq.contact_count-ca,'damage':before-hp(p),'result':str(fq.last_contact.result),
           'old_generation':old_gen,'current_generation':fq.get_generation(),'contact_generation':fq.last_contact.source_generation})
    gm.set_training_menu_open(False)

    # A training reset really does retire the projectile's old round.
    reset(600);yield from wait(.7);yield from ranged(q)
    fire=fq.fire_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    q.get_combat_input().notify_attack_released();yield from until(lambda:fq.fire_count>fire)
    gm.reset_training();ca=fq.contact_count;before=hp(p)
    yield from wait(1)
    check('T15 reset retires already-fired old-round projectile',fq.contact_count==ca and hp(p)==before and len(shots())==0)

    # PaidQ limited by the actual resource balance. No false Full on hold time.
    old_ce=fd.initial_cursed_energy;temp(fd,'initial_cursed_energy',10.)
    reset(600);yield from wait(.7);yield from ranged(q)
    full=fq.full_count;q.get_combat_input().notify_attack_pressed();yield from wait(10)
    check('T09 resource-limited hold never reports Full',fq.full_count==full and fq.last_action.paid_q<1,{'paid_q':fq.last_action.paid_q,'full_delta':fq.full_count-full})
    gm.reset_training();q.get_combat_input().notify_attack_released()
    fd.set_editor_property('initial_cursed_energy',old_ce)

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    q.character_movement.set_component_tick_enabled(True);p.character_movement.set_component_tick_enabled(True)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    for n,v in old_cvars.items():cmd(n+' '+str(v))
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    gm.jjk_clear_blockers();gm.set_training_settings(unreal.TrainingSettings());gm.reset_training();pc.set_combat_input_enabled(True)
    r['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        if time.monotonic()-started>200:raise TimeoutError()
        next(gen)
    except StopIteration:r['status']='passed' if all(c['passed'] for c in r['checks']) else 'failed';finish()
    except Exception:r['status']='failed';r['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
