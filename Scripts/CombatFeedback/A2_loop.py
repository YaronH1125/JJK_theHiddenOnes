"""One actual combat-to-result-to-restart loop, with a stationary AI fixture.

Domain starts with temporary energy 100; HP/CE/damage remain normal. AI decision
making is paused only for repeatable KO geometry. No debug kill or hit injection.
"""
from pathlib import Path
exec(compile((Path(__file__).parent/'A2_pie.py').read_text(encoding='utf-8').split('gen=suite();started=')[0],str(Path(__file__).parent/'A2_pie.py'),'exec'))
report['fixture']='Domain initial energy 100 only; normal HP/CE, no infinite resources. AI mode enables real match resolution; its decision controller is paused for reproducible attack geometry.'
freeze_ai=False
def place(gap):
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.stop_movement_immediately();f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def suite():
    global recording,freeze_ai
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0')
    temp(fd,'initial_energy',100.);reset();yield from wait(.4)
    unreal.AudioMixerLibrary.start_recording_output(w,90.);recording=True
    mark('loop-A1');c=fp.contact_count;p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>c)
    check('loop opens with actual A1',fp.last_contact.actual_damage==35);yield from wait(1)
    mark('loop-Guard');q.get_combat_input().notify_guard_pressed();yield from wait(.35);c=fp.contact_count
    p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>c)
    check('loop guard has real result and B montage',fp.last_contact.result==unreal.CombatFeedbackResult.GUARD and q.mesh.get_anim_instance().get_current_active_montage()==lib.guard)
    q.get_combat_input().notify_guard_released();yield from wait(1.2)
    place(600);yield from wait(.7);yield from stance(q);mark('loop-charge-cancel');fire=fq.fire_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.2)
    cancelled=q.request_dodge(unreal.Vector(0,1,0));yield from wait(.8)
    q.get_combat_input().notify_attack_released();yield
    check('loop legal dodge cancels charge without stale Fire/loop',cancelled and fq.fire_count==fire and qa.active_loop_count==0)
    place(600);yield from wait(.3);mark('loop-mobile');c=fq.contact_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4);q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>c)
    check('loop mobile actual contact joins visuals/audio',fq.contact_count==c+1 and fq.last_contact.result==unreal.CombatFeedbackResult.HIT and qv.active_cue_count>0 and qa.last_cue==K.RANGED_HIT)
    yield from wait(1.1)
    yield from until(lambda:q.get_fighter_attribute_set().cursed_energy.current_value>=fd.domain_config.orb_cost,12)
    mark('loop-domain');c=fq.contact_count;q.get_combat_input().notify_domain_pressed();yield from until(lambda:fq.contact_count>c,4)
    check('loop domain produces real orb contact',fq.contact_count>c and fq.last_contact.tier==unreal.CombatFeedbackTier.DOMAIN_ORB)
    yield from until(lambda:not tag(q,'State.DomainActive'),8)
    mark('loop-real-KO');gm.set_opponent_mode(unreal.OpponentMode.AI)
    ai=q.get_controller();assert isinstance(ai,unreal.FighterAIController);ai.deactivate_ai();freeze_ai=True
    place(120);yield from wait(.5);round_before=gm.get_feedback_round_id();hits=0
    while not q.is_dead() and hits<13:
        place(120);c=fp.contact_count;p.get_combat_input().submit_heavy_punch()
        yield from until(lambda:fp.contact_count>c,2)
        check('KO heavy '+str(hits+1)+' comes from actual input and sweep',fp.contact_count==c+1 and fp.last_contact.result==unreal.CombatFeedbackResult.HIT)
        hits+=1;yield from wait(1.1)
    yield from until(gm.is_match_resolved,2)
    check('real lethal damage resolves player win',q.is_dead() and hp(q)==0 and fp.last_contact.lethal and gm.is_match_resolved() and gm.get_match_outcome()==unreal.MatchOutcome.PLAYER_WIN)
    shot_image('loop-real-victory');yield from wait(.15)
    check('victory clears all consumer loops and pulses',all(x.active_loop_count==0 for x in [pa,qa]) and pv.active_cue_count==0 and qv.active_cue_count==0 and str(pp.pose_state)=='None' and str(qp.pose_state)=='None' and pc.combat_camera_feedback.applied_roll==0)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    freeze_ai=False
    gm.restart_match();gm.set_opponent_mode(unreal.OpponentMode.STATIC);pc.set_training_panel_open(False);pc.set_combat_input_enabled(True)
    place(120);yield from wait(.5)
    check('actual restart restores normal health/resources and unique bindings',not gm.is_match_resolved() and gm.get_feedback_round_id()>round_before and hp(p)==1000 and hp(q)==1000 and pc.get_feedback_binding_count()==2)
    for name,method,damage,reaction in [('A1','submit_light_attack',35,lib.light),('HeavyPunch','submit_heavy_punch',90,lib.heavy)]:
        mark('after-real-restart-'+name);before=hp(q);c=fp.contact_count;getattr(p.get_combat_input(),method)();yield from until(lambda:fp.contact_count>c,2)
        check('after real restart '+name+' works without another reset',fp.contact_count==c+1 and hp(q)==before-damage and q.mesh.get_anim_instance().get_current_active_montage()==reaction);yield from wait(1.3)
    place(600);yield from wait(.7);yield from stance(q);mark('after-real-restart-Super');r=qp.recoil_count;c=fq.contact_count;before=hp(p)
    q.get_combat_input().notify_kick_pressed();yield from until(lambda:fq.last_action.paid_q>.999,5)
    q.get_combat_input().notify_kick_released();yield from until(lambda:fq.contact_count>c,2)
    check('after real restart full super works without another reset',fq.contact_count==c+1 and hp(p)==before-220 and qp.recoil_count==r+1 and qv.active_cue_count>0)
    yield from wait(1.2)

gen=suite();started=time.monotonic()
def finish():
    global recording
    unreal.unregister_slate_post_tick_callback(handle)
    if recording:unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'A2-actual-result-loop',str(out));recording=False
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.set_training_menu_open(False);gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        # GameMode reactivates its AI when the victim recovers; keep this explicit
        # stationary fixture in effect across each recovery, without altering damage.
        if freeze_ai:
            ai=q.get_controller()
            if isinstance(ai,unreal.FighterAIController) and ai.is_ai_active():ai.deactivate_ai()
        sample()
        if time.monotonic()-started>180:raise TimeoutError('actual result loop')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
