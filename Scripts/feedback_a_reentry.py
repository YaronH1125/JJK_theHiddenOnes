"""Fresh-world ownership and domain clock smoke. Run three times with run_feedback_a.py reentry.

Uses real domain abilities/curved orbs and unchanged damage/energy costs.
The brief explicit stop request is labeled as a clock probe, not a real hit.
"""
from pathlib import Path
helper=Path(__file__).with_name('feedback_a_regression.py').read_text(encoding='utf-8')
exec(compile(helper.split('gen=suite();started=')[0],str(Path(__file__).with_name('feedback_a_regression.py')),'exec'))
out=Path(unreal.Paths.project_saved_dir())/'FeedbackA/reentry.json'
r={'status':'running','checks':[]}
def suite():
    check('T15 fresh world has exactly one dispatcher per fighter',len(p.get_components_by_class(unreal.CombatFeedbackComponent))==1 and len(q.get_components_by_class(unreal.CombatFeedbackComponent))==1)
    check('T15 fresh world has no stale projectiles/stop',not shots() and not fp.is_stopped() and not fq.is_stopped())
    temp(fd,'initial_energy',100.);reset(600);yield from wait(.7)
    cmd('JJK.Feedback.HitStop 1')
    count=fp.contact_count;fire=fp.fire_count;start=now();inp.notify_domain_pressed()
    yield from until(lambda:tag(p,'State.DomainActive'))
    domain_start=now();fp.debug_request_stop(.1)
    yield from until(lambda:fp.fire_count>fire)
    first_fire=fp.fire_count;old_gen=fp.get_generation();old_round=gm.get_feedback_round_id()
    gm.set_training_menu_open(True)
    yield from until(lambda:fp.contact_count>count)
    check('Domain real orb settles as domain Hit',fp.contact_count>count and fp.last_contact.tier==unreal.CombatFeedbackTier.DOMAIN_ORB and fp.last_contact.result==unreal.CombatFeedbackResult.HIT)
    check('Domain auto Fire and default zero hit stop',fp.fire_count>fire and not fp.is_stopped() and not fq.is_stopped())
    yield from until(lambda:not tag(p,'State.DomainActive'),8)
    duration=now()-domain_start
    check('Domain total duration stays on world clock',abs(duration-fd.domain_config.duration)<.12,{'world_duration':duration,'configured':fd.domain_config.duration})
    check('Domain menu cleanup preserves flight, resource refusal and End',not fp.is_current(old_round,old_gen)
          and fp.last_contact.source_generation==fp.get_generation() and fp.fire_count==first_fire
          and fp.last_action.stage==unreal.CombatActionStage.END,
          {'fire_after_menu':fp.fire_count-first_fire,'contact_generation':fp.last_contact.source_generation,'current_generation':fp.get_generation(),'last_stage':str(fp.last_action.stage)})
    gm.set_training_menu_open(False)
    settings=unreal.TrainingSettings();settings.set_editor_property('infinite_resources',True)
    gm.set_training_settings(settings);reset(600);yield from wait(.3)
    fire=fp.fire_count;inp.notify_domain_pressed();yield from until(lambda:fp.fire_count>fire)
    first_fire=fp.fire_count;gm.set_training_menu_open(True)
    yield from until(lambda:fp.fire_count>first_fire,4)
    check('Domain infinite-resource scheduler publishes later Fire after menu',fp.fire_count>first_fire
          and fp.last_action.stage==unreal.CombatActionStage.FIRE and fp.last_action.generation==fp.get_generation(),
          {'fire_after_menu':fp.fire_count-first_fire,'generation':fp.get_generation()})
    gm.set_training_menu_open(False);gm.set_training_settings(unreal.TrainingSettings())
    reset(600);yield from wait(.3)
    inp.notify_domain_pressed();q.get_combat_input().notify_domain_pressed()
    yield from wait(1.2)
    check('Dual domain suppresses orbs',not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DomainOrb))
    gm.reset_training();yield from wait(.2)
    check('T15 reset clears domain ownership',not tag(p,'State.DomainActive') and not tag(q,'State.DomainActive') and not unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DomainOrb))

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    for n,v in old_cvars.items():cmd(n+' '+str(v))
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    gm.set_training_menu_open(False)
    gm.set_training_settings(unreal.TrainingSettings())
    gm.reset_training();pc.set_combat_input_enabled(True)
    r['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        if time.monotonic()-started>40:raise TimeoutError()
        next(gen)
    except StopIteration:r['status']='passed' if all(c['passed'] for c in r['checks']) else 'failed';finish()
    except Exception:r['status']='failed';r['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
