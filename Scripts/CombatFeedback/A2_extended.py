"""A2 saved integration: super-blast endings and animation priority from actual gameplay."""
from pathlib import Path
exec(compile((Path(__file__).parent/'A2_pie.py').read_text(encoding='utf-8').split('gen=suite();started=')[0],str(Path(__file__).parent/'A2_pie.py'),'exec'))
def wall(x,y=0,width=.3):
    before={a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)}
    gm.jjk_spawn_blocker(float(x),float(y),float(width),6.,5.)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.get_name() not in before and a.get_component_by_class(unreal.StaticMeshComponent):a.set_actor_location(unreal.Vector(x,y,250),False,True)
def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0')
    for result in ['guard','immune','wall','expire','muzzle']:
        reset(600);yield from wait(.7);yield from stance(q);mark('Super-'+result)
        c=fq.contact_count;fire=fq.fire_count;r=qp.recoil_count;v=qv.spawned_cue_count;before=hp(p)
        if result=='guard':p.get_combat_input().notify_guard_pressed()
        if result=='expire':
            for f,x in [(p,0),(q,600)]:f.set_actor_location(unreal.Vector(x,0,5000),False,True);f.character_movement.set_component_tick_enabled(False)
        q.get_combat_input().notify_kick_pressed();yield from until(lambda:fq.last_action.paid_q>.999,5)
        if result=='wall':wall(300)
        if result=='muzzle':
            muzzle=q.mesh.get_socket_location(fd.muzzle_socket);wall(muzzle.x-15,muzzle.y,.2)
        q.get_combat_input().notify_kick_released()
        if result=='immune':
            yield from wait(.24);p.request_dodge(unreal.Vector(0,1,0));p.character_movement.set_component_tick_enabled(False)
        if result=='muzzle':
            yield from until(lambda:fq.last_action.stage==unreal.CombatActionStage.END,2)
            check('super muzzle refusal has no Fire, recoil or visual impact',fq.fire_count==fire and qp.recoil_count==r and qv.spawned_cue_count==v and fq.last_action.end_reason==unreal.CombatFeedbackEnd.REJECTED)
            check('super rejected session stops D charge loop',qa.active_loop_count==0)
        else:
            yield from until(lambda:fq.fire_count>fire,2)
            check('super '+result+' one real Fire/recoil',fq.fire_count==fire+1 and qp.recoil_count==r+1)
            if result=='expire':
                p.set_actor_location(unreal.Vector(0,1000,100),False,True)
                yield from until(lambda:fq.last_lifecycle.reason==unreal.CombatFeedbackEnd.EXPIRE,6)
                check('super expiry is lifecycle only with C/D expiry',fq.contact_count==c and qv.spawned_cue_count==v+1 and str(qv.last_cue_effect)=='PS_C_Expire_Laser' and qa.last_cue==K.EXPIRE)
            else:
                yield from until(lambda:fq.contact_count>c,2)
                expected={'guard':unreal.CombatFeedbackResult.GUARD,'immune':unreal.CombatFeedbackResult.IMMUNE,'wall':unreal.CombatFeedbackResult.WORLD_IMPACT}[result]
                kind={'guard':K.GUARD,'immune':K.IMMUNE,'wall':K.WORLD_IMPACT}[result]
                check('super '+result+' true result has one C/D cue and no flesh damage',fq.contact_count==c+1 and fq.last_contact.result==expected and qv.spawned_cue_count==v+1 and qa.last_cue==kind and hp(p)==before,
                      dict(result=str(fq.last_contact.result),damage=before-hp(p),cue=str(qa.last_cue),visuals=qv.spawned_cue_count-v))
                if result=='guard':check('super guard selects B impact',p.mesh.get_anim_instance().get_current_active_montage()==lib.guard)
            yield from wait(1)
            check('super '+result+' no persistent loop/FX/recoil',qa.active_loop_count==0 and qv.active_cue_count==0 and str(qp.pose_state)=='None')
        gm.jjk_clear_blockers()
        for f in [p,q]:f.character_movement.set_component_tick_enabled(True)
    # Actual back hit bypasses the guard arc and must replace the owned loop.
    reset();yield from wait(.3);q.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
    q.get_combat_input().notify_guard_pressed();yield from wait(.35);c=fp.contact_count;e=qp.guard_end_count
    p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>c)
    check('back hit interrupts guard loop with real B light reaction',fp.last_contact.result==unreal.CombatFeedbackResult.HIT and q.mesh.get_anim_instance().get_current_active_montage()==lib.light and not q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop))
    yield
    check('hit priority never inserts guard-end pose',qp.guard_end_count==e and str(qp.pose_state)=='None')
    cmd('JJK.Feedback.Reaction 0');yield
    check('Reaction off clears active library impact without clearing stun',not q.mesh.get_anim_instance().montage_is_playing(lib.light) and tag(q,'State.HitStun'))
    cmd('JJK.Feedback.Reaction 1')
    temp(fd,'initial_health',20.);reset();yield from wait(.3);q.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
    q.get_combat_input().notify_guard_pressed();yield from wait(.35);e=qp.guard_end_count
    p.get_combat_input().submit_light_attack();yield from until(q.is_dead);yield
    check('actual lethal back hit cleans guard without end overlay',q.is_dead() and str(qp.pose_state)=='None' and qp.guard_end_count==e)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    reset();yield from wait(.3);q.get_combat_input().notify_guard_pressed();yield from wait(.35);gm.set_training_menu_open(True);yield
    check('menu cleans owned guard pose',str(qp.pose_state)=='None' and not q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop))

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    gm.set_training_menu_open(False);gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        sample()
        if time.monotonic()-started>180:raise TimeoutError('extended suite')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
