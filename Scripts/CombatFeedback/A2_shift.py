"""Actual charge/windup Shift cancellation; rejection preserves charge, fired shots survive."""
from pathlib import Path
exec(compile((Path(__file__).parent/'A2_pie.py').read_text(encoding='utf-8').split('gen=suite();started=')[0],str(Path(__file__).parent/'A2_pie.py'),'exec'))
def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0')
    for kind in ['mobile','super']:
        for phase in ['charging','windup']:
            reset(600);yield from wait(.7);yield from stance(q);mark(kind+'-'+phase+'-Shift')
            press=q.get_combat_input().notify_attack_pressed if kind=='mobile' else q.get_combat_input().notify_kick_pressed
            release=q.get_combat_input().notify_attack_released if kind=='mobile' else q.get_combat_input().notify_kick_released
            speed=q.character_movement.max_walk_speed;fire=fq.fire_count;recoil=qp.recoil_count
            press();yield from wait(.3 if kind=='mobile' else 1.4);session=fq.last_action.session_id
            if phase=='windup':release();yield from wait(.025)
            resource=q.get_action_resource();accepted=q.request_dodge(unreal.Vector(0,1,0));paid=q.get_fighter_attribute_set().cursed_energy.current_value
            check(kind+' '+phase+' successful Shift pays once',accepted and abs(resource-q.get_action_resource()-fd.dodge_config.dodge_cost)<.01)
            check(kind+' '+phase+' synchronously ends ability/session/loop',not tag(q,'State.BlastCharging') and qa.active_loop_count==0 and fq.last_action.stage==unreal.CombatActionStage.END and fq.last_action.session_id==session and fq.last_action.end_reason==unreal.CombatFeedbackEnd.INTERRUPTED,
                  dict(stage=str(fq.last_action.stage),reason=str(fq.last_action.end_reason),loops=qa.active_loop_count,charging_tag=tag(q,'State.BlastCharging')))
            yield from wait(.8);release();yield
            check(kind+' '+phase+' stale release cannot Fire/recoil or keep charging cost',fq.fire_count==fire and qp.recoil_count==recoil and q.get_fighter_attribute_set().cursed_energy.current_value>=paid-.01)
            check(kind+' '+phase+' restores original movement speed',abs(q.character_movement.max_walk_speed-speed)<.01)
            # New mobile input must create a fresh paid-Q session, even after a cancelled super.
            place=q.get_actor_location();q.set_actor_location(unreal.Vector(600,0,100),False,True)
            q.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False);yield from wait(.2)
            c=fq.contact_count;q.get_combat_input().notify_attack_pressed();yield from wait(.4)
            fresh=fq.last_action.session_id;q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>c,2)
            check(kind+' '+phase+' next press creates fresh session and only its shot',fresh>session and fq.fire_count==fire+1 and fq.contact_count==c+1 and fq.last_contact.paid_q<.6,
                  dict(old_session=session,new_session=fresh,fire_delta=fq.fire_count-fire,paid_q=fq.last_contact.paid_q))
    for kind in ['mobile','super']:
        reset(600);yield from wait(.7);yield from stance(q);mark(kind+'-rejected-airborne-Shift')
        press=q.get_combat_input().notify_attack_pressed if kind=='mobile' else q.get_combat_input().notify_kick_pressed
        release=q.get_combat_input().notify_attack_released if kind=='mobile' else q.get_combat_input().notify_kick_released
        fire=fq.fire_count;press();yield from wait(.3 if kind=='mobile' else 1.4)
        q.character_movement.set_movement_mode(unreal.MovementMode.MOVE_FALLING)
        q.character_movement.set_component_tick_enabled(False);resource=q.get_action_resource()
        accepted=q.request_dodge(unreal.Vector(0,1,0))
        check(kind+' rejected Shift keeps paid charge/loop/resources',not accepted and tag(q,'State.BlastCharging') and qa.active_loop_count==1 and abs(resource-q.get_action_resource())<.01)
        q.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING);q.character_movement.set_component_tick_enabled(True)
        release();yield from until(lambda:fq.fire_count>fire,2)
        check(kind+' original release still fires after rejection',fq.fire_count==fire+1)
    reset(600);yield from wait(.7);yield from stance(q);mark('already-fired-Shift');fire=fq.fire_count;c=fq.contact_count
    q.get_combat_input().notify_kick_pressed();yield from wait(1.4);q.get_combat_input().notify_kick_released();yield from until(lambda:fq.fire_count>fire,2)
    accepted=q.request_dodge(unreal.Vector(0,1,0));yield from until(lambda:fq.contact_count>c,2)
    check('successful Shift after Fire preserves actual in-flight projectile',accepted and fq.fire_count==fire+1 and fq.contact_count==c+1 and fq.last_contact.result==unreal.CombatFeedbackResult.HIT)

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for f in [p,q]:f.character_movement.set_component_tick_enabled(True)
    gm.set_training_menu_open(False);gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        sample()
        if time.monotonic()-started>150:raise TimeoutError('Shift regression')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
