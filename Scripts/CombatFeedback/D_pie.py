"""D real combat tests. D_run.py starts this in a fresh PIE with temporary consumer binding.

Real cases use gameplay input; CONTRACT cases replay independent snapshots only
inside D's consumer and never inject gameplay settlement. Fixtures are restored.
Raw Master mixer recording covers the initial samples; full holds are measured separately.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fp,fq=p.get_combat_feedback(),q.get_combat_feedback()
ap=p.get_component_by_class(unreal.CombatAudioConsumer);aq=q.get_component_by_class(unreal.CombatAudioConsumer)
fd=p.get_definition();old_ce=fd.initial_cursed_energy;old_energy=fd.initial_energy
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'FeedbackD'
K=unreal.CombatAudioCueKind
report={'status':'running','checks':[],'cases':[],'sample_timeline':[],
        'test_binding':'temporary D_setup; A final saved assignment pending',
        'engine':unreal.SystemLibrary.get_engine_version(), 'human_listening':'not performed',
        'contract_probe_note':'CONTRACT entries use local consumer-only replay; never gameplay settlement or real-hit evidence'}
recording=False
report_name='contract-report.json' if globals().get('D_CONTRACT_ONLY',False) else 'pie-report.json'
def write():(out/report_name).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,d=None):report['checks'].append({'name':n,'passed':bool(b),'detail':d});write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s)
def hp(f):return f.get_fighter_attribute_set().health.current_value
def count(a,k):return a.get_cue_play_count(k)
def dist(a,b):return ((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)**.5
def reset(gap=120):
    gm.set_training_menu_open(False);gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    for actor,x,yaw in [(p,0,0),(q,gap,180)]:
        actor.character_movement.set_component_tick_enabled(True)
        actor.character_movement.stop_movement_immediately()
        actor.set_actor_location(unreal.Vector(x,0,100),False,True)
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def stance(f):f.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
def marker(label):report['sample_timeline'].append({'label':label,'game_seconds':now()});write()
def wall(x=300):
    old={a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)}
    gm.jjk_spawn_blocker(float(x),0.,.3,6.,5.)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.get_name() not in old and a.get_component_by_class(unreal.StaticMeshComponent):
            a.set_actor_location(unreal.Vector(x,0,250),False,True)

def melee(label,method,kind,result='hit',expected=35):
    reset(600 if result=='whiff' else 120);yield from wait(.35)
    if result=='guard':q.get_combat_input().notify_guard_pressed()
    if result=='immune':q.request_dodge(unreal.Vector(0,1,0));q.character_movement.set_component_tick_enabled(False)
    before=hp(q);contact=fp.contact_count;hits=count(ap,kind);swing=count(ap,K.KICK_SWING if 'kick' in method else K.PUNCH_SWING)
    marker(label);getattr(p.get_combat_input(),method)()
    if result=='whiff':yield from wait(1.2)
    else:yield from until(lambda:fp.contact_count>contact,2)
    chosen=K.GUARD if result=='guard' else K.IMMUNE if result=='immune' else kind
    row={'case':label,'contact_delta':fp.contact_count-contact,'result':str(fp.last_contact.result),
         'last_cue':str(ap.last_cue),'sound':str(ap.last_sound),'damage':before-hp(q),
         'cue_position':str(ap.last_cue_location),'contact_position':str(fp.last_contact.location)}
    report['cases'].append(row);write()
    if result=='whiff':
        check(label+' has only calibrated swing',fp.contact_count==contact and count(ap,kind)==hits and count(ap,K.PUNCH_SWING)==swing+1,row)
    else:
        expected_result={'hit':unreal.CombatFeedbackResult.HIT,'guard':unreal.CombatFeedbackResult.GUARD,'immune':unreal.CombatFeedbackResult.IMMUNE}[result]
        check(label+' uses settled result and cue',fp.contact_count==contact+1 and fp.last_contact.result==expected_result and ap.last_cue==chosen,row)
        check(label+' plays at actual contact',dist(ap.last_cue_location,fp.last_contact.location)<1,row)
        if result=='hit':check(label+' preserves damage',abs(before-hp(q)-expected)<.01,row)
        if result in ['guard','immune']:check(label+' does not play flesh',count(ap,kind)==hits,row)
    yield from wait(1.1)
    check(label+' owns no trailing audio component',ap.active_one_shot_count==0 and ap.active_loop_count==0)
    q.character_movement.set_component_tick_enabled(True);q.get_combat_input().notify_guard_released()

def shot(label,kind='mobile',result='hit',charge=.4):
    reset(600);yield from wait(.7);yield from stance(q)
    if result=='guard':p.get_combat_input().notify_guard_pressed()
    if result=='expire':
        for a,x in [(p,0),(q,600)]:
            a.set_actor_location(unreal.Vector(x,0,5000),False,True);a.character_movement.set_component_tick_enabled(False)
    fire=fq.fire_count;contacts=fq.contact_count;before=hp(p)
    firekind=K.SUPER_FIRE if kind=='super' else K.MOBILE_FIRE
    oldfire=count(aq,firekind)
    marker(label);inp=q.get_combat_input()
    (inp.notify_kick_pressed if kind=='super' else inp.notify_attack_pressed)();yield from wait(charge)
    check(label+' owns one actual charge loop',aq.active_loop_count==1 and aq.active_charge_session>0,aq.active_loop_count)
    if result=='wall':wall()
    (inp.notify_kick_released if kind=='super' else inp.notify_attack_released)()
    yield from until(lambda:fq.fire_count>fire,2)
    check(label+' successful Fire owns one muzzle cue and stops loop',fq.fire_count==fire+1 and count(aq,firekind)==oldfire+1 and aq.active_loop_count==0,
          {'fire_delta':fq.fire_count-fire,'audio_fire_delta':count(aq,firekind)-oldfire,'loops':aq.active_loop_count})
    if result=='expire':p.set_actor_location(unreal.Vector(0,1000,100),False,True)
    if result=='expire':yield from until(lambda:aq.last_cue==K.EXPIRE,5)
    else:yield from until(lambda:fq.contact_count>contacts,3)
    chosen={'hit':K.RANGED_HIT,'guard':K.GUARD,'wall':K.WORLD_IMPACT,'expire':K.EXPIRE}[result]
    row={'case':label,'fire_delta':fq.fire_count-fire,'contact_delta':fq.contact_count-contacts,'last_cue':str(aq.last_cue),
         'sound':str(aq.last_sound),'damage':before-hp(p),'paid_q':fq.last_contact.paid_q if result!='expire' else None}
    report['cases'].append(row);write()
    check(label+' chooses contact/world/expiry sound',aq.last_cue==chosen,row)
    if result=='expire':check(label+' expiry is not a hit',fq.contact_count==contacts and hp(p)==before,row)
    else:check(label+' contact point matches audio',dist(aq.last_cue_location,fq.last_contact.location)<1,row)
    yield from wait(.9)
    check(label+' bounded tails return to zero',aq.active_loop_count==0 and aq.active_one_shot_count==0)
    p.get_combat_input().notify_guard_released()
    for a in [p,q]:a.character_movement.set_component_tick_enabled(True)
    gm.jjk_clear_blockers()

def contracts():
    # Explicit consumer contracts. These locally replay snapshots from genuine events,
    # do not call A's dispatcher/settlement, and are not reported as real attacks.
    reset();yield from wait(.35);c=fp.contact_count;p.get_combat_input().submit_light_attack()
    yield from until(lambda:fp.contact_count>c)
    snapshot=fp.last_contact.copy();played=ap.played_cue_count;dup=ap.duplicate_count
    ap.debug_consume_contact(snapshot)
    check('CONTRACT duplicate contact is ignored locally',ap.played_cue_count==played and ap.duplicate_count==dup+1)
    for i in range(10):
        snapshot.set_editor_property('attack_instance_id',9000000+i)
        ap.debug_consume_contact(snapshot)
    yield from wait(.06)
    impacts=[a for a in p.get_components_by_class(unreal.AudioComponent) if a.is_playing() and a.sound and a.sound.get_name().startswith('SW_D_PunchHit')]
    check('CONTRACT impact concurrency retires excess voices',len(impacts)<=4,{'playing_impact_voices':len(impacts),'all_one_shots_including_swing':ap.active_one_shot_count})
    # Same shared Impact group, alternate owner, with player-victim priority.
    priority_snapshot=snapshot.copy()
    priority_snapshot.set_editor_property('source',q);priority_snapshot.set_editor_property('target',p)
    priority_snapshot.set_editor_property('source_generation',fq.get_generation())
    priority_snapshot.set_editor_property('target_generation',fp.get_generation())
    for i in range(4):
        priority_snapshot.set_editor_property('attack_instance_id',9100000+i)
        aq.debug_consume_contact(priority_snapshot)
    impacts=[a for owner in [p,q] for a in owner.get_components_by_class(unreal.AudioComponent)
             if a.is_playing() and a.sound and a.sound.get_name().startswith('SW_D_PunchHit')]
    check('CONTRACT shared Impact cap spans both owners and protects player victim',len(impacts)==4 and
          all(a.get_owner()==q and a.get_editor_property('priority')>=5 for a in impacts),
          {'playing_impact_voices':len(impacts),'p_stolen':ap.stolen_voice_count})
    dropped=ap.dropped_cue_count;played=ap.played_cue_count
    snapshot.set_editor_property('attack_instance_id',9200000)
    ap.debug_consume_contact(snapshot)
    check('CONTRACT low priority tail cannot steal player impact',ap.dropped_cue_count==dropped+1 and ap.played_cue_count==played,
          {'dropped_delta':ap.dropped_cue_count-dropped})
    check('CONTRACT replay never mutates public contact snapshot',fp.last_contact.attack_instance_id<9000000)
    yield from wait(.8)
    reset(600);yield from wait(.7);yield from stance(q)
    q.get_combat_input().notify_attack_pressed();yield from wait(.25)
    old_action=fq.last_action.copy();old_session=aq.active_charge_session
    q.get_combat_input().invalidate_session('D old session');q.get_combat_input().release_continuous_inputs();yield from wait(.3)
    q.get_combat_input().notify_attack_pressed();yield from wait(.25)
    new_session=aq.active_charge_session
    old_action.set_editor_property('generation',fq.get_generation())
    old_action.set_editor_property('stage',unreal.CombatActionStage.END)
    old_action.set_editor_property('end_reason',unreal.CombatFeedbackEnd.CANCEL)
    aq.debug_consume_action(old_action)
    check('CONTRACT same-generation old End cannot stop new charge',old_session!=new_session and new_session>0 and
          aq.active_charge_session==new_session and aq.is_charge_loop_playing(),
          {'old_session':old_session,'new_session':new_session,'after_session':aq.active_charge_session,'playing':aq.is_charge_loop_playing()})
    stale=fq.last_action.copy();gm.reset_training();yield from wait(.1);played=aq.played_cue_count
    stale.set_editor_property('stage',unreal.CombatActionStage.START)
    aq.debug_consume_action(stale)
    check('CONTRACT stale round/generation cannot create loop',aq.active_loop_count==0 and aq.played_cue_count==played)

def suite():
    global recording
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.Audio 1');cmd('JJK.Feedback.AudioGain 1');cmd('JJK.Feedback.AudioLog 1')
    check('D consumers are unique, own hard profile',bool(ap and aq and ap.audio_profile and aq.audio_profile) and
          len(p.get_components_by_class(unreal.CombatAudioConsumer))==1 and len(q.get_components_by_class(unreal.CombatAudioConsumer))==1,
          {'p':str(ap),'q':str(aq),'p_profile':str(ap.audio_profile) if ap else None,'q_profile':str(aq.audio_profile) if aq else None,
           'p_components':len(p.get_components_by_class(unreal.CombatAudioConsumer)),'q_components':len(q.get_components_by_class(unreal.CombatAudioConsumer))})
    assert report['checks'][-1]['passed'], 'Cold-start AudioProfile binding missing'
    report['audio_device']=list(unreal.CombatAudioDiagnostics.describe_audio_device(w));write()
    if globals().get('D_CONTRACT_ONLY',False):
        yield from contracts()
        return
    unreal.AudioMixerLibrary.start_recording_output(w,50.);recording=True
    report['recording_start_game_seconds']=now();write()
    yield from melee('A1 whiff','submit_light_attack',K.PUNCH_HIT,'whiff')
    yield from melee('A1 hit','submit_light_attack',K.PUNCH_HIT)
    yield from melee('A1 guard','submit_light_attack',K.PUNCH_HIT,'guard')
    yield from melee('A1 immune','submit_light_attack',K.PUNCH_HIT,'immune')
    yield from melee('Kick hit','submit_kick',K.KICK_HIT,expected=45)
    yield from melee('Heavy punch','submit_heavy_punch',K.HEAVY_HIT,expected=90)
    yield from shot('Mobile weak')
    yield from shot('Mobile guard',result='guard')
    yield from shot('Mobile wall',result='wall')
    yield from shot('Super full',kind='super',charge=3.3)
    unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'D-real-combat',str(out));recording=False
    report['raw_sample']=str(out/'D-real-combat.wav');report['recording_end_game_seconds']=now();write()
    yield from shot('Mobile expiry',result='expire')

    # Consecutive same-class real contacts must select different actual files.
    previous=None
    for i in range(4):
        reset();yield from wait(.3);old=fp.contact_count;p.get_combat_input().submit_light_attack()
        yield from until(lambda:fp.contact_count>old)
        name=str(ap.last_sound)
        check('D-T02 punch variant '+str(i)+' avoids prior file',name!=previous,{'previous':previous,'current':name})
        previous=name;yield from wait(.9)

    # Actual PaidQ and loop ownership measured for BOTH manual abilities.
    for kind in ['mobile','super']:
        reset(600);yield from wait(.7);yield from stance(q)
        full=count(aq,K.CHARGE_FULL);loops=aq.loop_start_count
        inp=q.get_combat_input();(inp.notify_kick_pressed if kind=='super' else inp.notify_attack_pressed)()
        yield from wait(3.4)
        gain,pitch,paid=aq.loop_gain,aq.loop_pitch,aq.loop_paid_q
        check('D-T03 '+kind+' reaches real full once',paid>.999 and count(aq,K.CHARGE_FULL)==full+1,{'paid_q':paid,'full_delta':count(aq,K.CHARGE_FULL)-full})
        replay=fq.last_action.copy();replay.set_editor_property('stage',unreal.CombatActionStage.FULL)
        aq.debug_consume_action(replay);aq.debug_consume_action(replay)
        check('CONTRACT '+kind+' repeated Full is ignored',count(aq,K.CHARGE_FULL)==full+1)
        yield from wait(60.)
        check('D-T03 '+kind+' 60s full is stable and one instance',aq.active_loop_count==1 and aq.is_charge_loop_playing() and aq.loop_start_count==loops+1 and
              count(aq,K.CHARGE_FULL)==full+1 and abs(aq.loop_gain-gain)<.001 and abs(aq.loop_pitch-pitch)<.001,
              {'loop_starts':aq.loop_start_count-loops,'full_delta':count(aq,K.CHARGE_FULL)-full,'gain':aq.loop_gain,'pitch':aq.loop_pitch})
        gm.reset_training();(inp.notify_kick_released if kind=='super' else inp.notify_attack_released)();yield from wait(.15)
        check('D-T04 '+kind+' reset clears old hold',aq.active_loop_count==0 and aq.active_one_shot_count==0)

    for kind in ['mobile','super']:
        fd.set_editor_property('initial_cursed_energy',55. if kind=='super' else 10.)
        reset(600);yield from wait(.7);yield from stance(q)
        full=count(aq,K.CHARGE_FULL)
        (q.get_combat_input().notify_kick_pressed if kind=='super' else q.get_combat_input().notify_attack_pressed)()
        yield from wait(3.6)
        check('D-T03 '+kind+' limited resources never fake Full',aq.active_loop_count==1 and aq.loop_paid_q<.999 and count(aq,K.CHARGE_FULL)==full,
              {'paid_q':aq.loop_paid_q,'full_delta':count(aq,K.CHARGE_FULL)-full,'gain':aq.loop_gain,'active_loop':aq.active_loop_count})
        gm.reset_training();q.get_combat_input().notify_attack_released();q.get_combat_input().notify_kick_released()
        fd.set_editor_property('initial_cursed_energy',old_ce)

    for mode in ['menu','input-disable','interrupt','death','reset']:
        reset(600);yield from wait(.7);yield from stance(q)
        q.get_combat_input().notify_attack_pressed();yield from wait(.35)
        fire=fq.fire_count;check('D-T04 '+mode+' started loop',aq.active_loop_count==1)
        if mode=='menu':gm.set_training_menu_open(True)
        elif mode=='input-disable':
            pc.set_combat_input_enabled(False)
            q.get_combat_input().invalidate_session('D test focus release');q.get_combat_input().release_continuous_inputs()
        elif mode=='interrupt':q.jjk_debug_force_hit_react()
        elif mode=='death':q.jjk_debug_kill();yield from until(lambda:q.is_dead(),1.)
        else:gm.reset_training()
        q.get_combat_input().notify_attack_released();yield from wait(.8)
        check('D-T04 '+mode+' no loop/tail/stale release fire',aq.active_loop_count==0 and aq.active_one_shot_count==0 and fq.fire_count==fire,
              {'loop':aq.active_loop_count,'tail':aq.active_one_shot_count,'fire_delta':fq.fire_count-fire})

    # Two owners charge concurrently. Ending q must preserve p's actual session.
    reset(600);yield from wait(.7);yield from stance(p);yield from stance(q)
    p.get_combat_input().notify_attack_pressed();q.get_combat_input().notify_attack_pressed();yield from wait(.4)
    ps=ap.active_charge_session
    check('D-T05 both fighters own one charge loop',ap.active_loop_count==1 and aq.active_loop_count==1)
    q.get_combat_input().invalidate_session('D test owner cancel');q.get_combat_input().release_continuous_inputs();yield from wait(.1)
    check('D-T05 q cancellation preserves p session',aq.active_loop_count==0 and ap.active_loop_count==1 and ap.active_charge_session==ps)
    gm.reset_training();yield from wait(.1)

    # Domain uses actual scheduler Fire, without any manual charge loop.
    fd.set_editor_property('initial_energy',100.)
    reset(600);yield from wait(.7);fire=fp.fire_count;full=count(ap,K.CHARGE_FULL);domain=count(ap,K.DOMAIN_START)
    p.get_combat_input().notify_domain_pressed();yield from until(lambda:fp.fire_count>fire,3)
    check('D-T05 domain start and real Fire have no manual loop/full',fp.fire_count>fire and count(ap,K.DOMAIN_START)==domain+1 and
          ap.active_loop_count==0 and count(ap,K.CHARGE_FULL)==full)
    q.get_combat_input().notify_domain_pressed();yield from wait(.6)
    check('D-T05 dual domains never introduce manual loops',ap.active_loop_count==0 and aq.active_loop_count==0)
    gm.reset_training();fd.set_editor_property('initial_energy',old_energy)

    reset();yield from wait(.7);before=hp(q);played=ap.played_cue_count;c=fp.contact_count
    cmd('JJK.Feedback.Audio 0');p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>c,2)
    check('D-T07 audio off preserves hit damage with no voices',abs(before-hp(q)-35)<.01 and ap.played_cue_count==played and ap.active_one_shot_count==0,
          {'damage':before-hp(q),'contact_delta':fp.contact_count-c,'played_delta':ap.played_cue_count-played})
    cmd('JJK.Feedback.Audio 1')
    reset();yield from wait(.35)
    saved_profile=ap.audio_profile;ap.set_editor_property('audio_profile',None)
    before=hp(q);p.get_combat_input().submit_light_attack();yield from wait(.8)
    check('D-T07 missing profile keeps gameplay alive without voices',abs(before-hp(q)-35)<.01 and ap.active_loop_count==0 and ap.active_one_shot_count==0)
    ap.set_editor_property('audio_profile',saved_profile)

    yield from contracts()
    for i in range(20):
        reset(600);yield from wait(.7);yield from stance(q)
        q.get_combat_input().notify_attack_pressed();yield from wait(.08)
        gm.reset_training();q.get_combat_input().notify_attack_released();yield from wait(.04)
        check('D-T07 reset '+str(i+1)+' returns all owned audio to zero',ap.active_loop_count==0 and aq.active_loop_count==0 and ap.active_one_shot_count==0 and aq.active_one_shot_count==0)
    check('D-T07 missing assets did not occur',ap.missing_cue_count==0 and aq.missing_cue_count==0,
          {'p':ap.missing_cue_count,'q':aq.missing_cue_count})

gen=suite();start=time.monotonic()
def finish():
    global recording
    unreal.unregister_slate_post_tick_callback(handle)
    if recording:
        unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'D-interrupted-combat',str(out));recording=False
    fd.set_editor_property('initial_cursed_energy',old_ce);fd.set_editor_property('initial_energy',old_energy)
    for a in [p,q]:a.character_movement.set_component_tick_enabled(True)
    gm.set_training_menu_open(False);gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-start;write()
def tick(dt):
    try:
        if time.monotonic()-start>340:raise TimeoutError('D PIE callback')
        next(gen)
    except StopIteration:report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
