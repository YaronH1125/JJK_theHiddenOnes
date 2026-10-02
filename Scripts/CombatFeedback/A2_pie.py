"""Saved B/C/D/E integration: real inputs, real sweeps/projectiles; no injected contacts."""
import json
from pathlib import Path
import time
import traceback
import unreal

out=Path(json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackA2/latest-run.json').read_text(encoding='utf-8'))['path'])
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0); gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter(); fd=p.get_definition(); lib=fd.reaction_library
fp,fq=p.get_combat_feedback(),q.get_combat_feedback()
pp,qp=[f.get_component_by_class(unreal.CombatPoseConsumer) for f in [p,q]]
pa,qa=[f.get_component_by_class(unreal.CombatAudioConsumer) for f in [p,q]]
pv,qv=[f.get_component_by_class(unreal.CombatRangedVisualConsumer) for f in [p,q]]
K=unreal.CombatAudioCueKind
report={'status':'running','binding':'saved production DA_Fighter_Ishigori; no temporary consumers or reaction library',
        'engine':unreal.SystemLibrary.get_engine_version(),'checks':[],'contacts':[],'timeline':[],
        'human_listening':'not performed','human_input':'not performed; actual shared input API drives abilities',
        'hit_stop_default':0,'captures':[]}
original=[]; recording=False; label=''; last_contacts=[fp.contact_count,fq.contact_count]
def write():(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,d=None):report['checks'].append(dict(name=n,passed=bool(b),detail=d));write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def tag(f,n):
    t=unreal.GameplayTag();t.import_text('(TagName="'+n+'")');return f.has_combat_tag(t)
def hp(f):return f.get_fighter_attribute_set().health.current_value
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s,pc)
def temp(o,k,v):
    if not any(a==o and b==k for a,b,c in original):original.append((o,k,o.get_editor_property(k)))
    o.set_editor_property(k,v)
def mark(n):
    global label
    label=n;report['timeline'].append(dict(label=n,game_seconds=now(),wall_seconds=time.monotonic()-started));write()
def reset(gap=120):
    pc.handle_app_activation_changed(True);pc.set_training_panel_open(False);gm.set_training_menu_open(False)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC);gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    pc.set_control_rotation(unreal.Rotator(pitch=-8,yaw=55,roll=0))
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.set_component_tick_enabled(True);f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def shot_image(name):
    path=out/(name+'.png');cmd('Shot showui -nosuffix filename='+path.as_posix())
    report['captures'].append(str(path));write()
def stance(f):f.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
def sample():
    for i,(f,fb) in enumerate([(p,fp),(q,fq)]):
        if fb.contact_count==last_contacts[i]:continue
        last_contacts[i]=fb.contact_count;c=fb.last_contact;t=c.target
        anim=t.mesh.get_anim_instance() if t else None;m=anim.get_current_active_montage() if anim else None
        report['contacts'].append(dict(case=label,source=f.get_name(),game_seconds=now(),attack=c.attack_instance_id,segment=c.segment_id,
             tier=str(c.tier),result=str(c.result),move=str(c.move_id),damage=c.actual_damage,paid_q=c.paid_q,
             reaction=m.get_name() if m else None,location=str(c.location),normal=str(c.normal),fallback=c.location_fallback))
def melee(name,method,damage,cue,reaction):
    reset();yield from wait(.35);mark(name);before=hp(q);c=fp.contact_count;a=pa.get_cue_play_count(cue);v=pv.spawned_cue_count
    getattr(p.get_combat_input(),method)();yield from until(lambda:fp.contact_count>c,2)
    check(name+' actual contact, original damage, one sound',fp.contact_count==c+1 and hp(q)==before-damage and pa.get_cue_play_count(cue)==a+1)
    check(name+' B reaction and no melee particles',q.mesh.get_anim_instance().get_current_active_montage()==reaction and pv.spawned_cue_count==v)
    check(name+' no knockdown or movement kick',not tag(q,'State.KnockedDown') and abs(q.character_movement.velocity.x)<.1)
    shot_image(name);yield from wait(1.2)
    check(name+' releases reaction, sound, and movement lock',q.can_act() and pa.active_one_shot_count==0 and not q.mesh.get_anim_instance().montage_is_playing(reaction))
def super_sample(name='prototype-SuperBlast'):
    reset(600);yield from wait(.7);yield from stance(q);mark(name)
    fire=fq.fire_count;c=fq.contact_count;r=qp.recoil_count;a=qa.get_cue_play_count(K.SUPER_FIRE);v=qv.spawned_cue_count;before=hp(p)
    q.get_combat_input().notify_kick_pressed();yield from until(lambda:fq.last_action.paid_q>.999,5)
    check(name+' has one charge loop, no early recoil',qa.active_loop_count==1 and qp.recoil_count==r)
    q.get_combat_input().notify_kick_released();yield from until(lambda:fq.fire_count>fire,2)
    check(name+' successful Fire plays B recoil once',fq.fire_count==fire+1 and qp.recoil_count==r+1 and q.mesh.get_anim_instance().montage_is_playing(qp.super_recoil),dict(fires=fq.fire_count-fire,recoils=qp.recoil_count-r,can_act=q.can_act()))
    shot_image(name+'-Fire')
    yield from until(lambda:fq.contact_count>c,2)
    check(name+' real full Hit=220 and one D Fire/C contact',hp(p)==before-220 and fq.contact_count==c+1 and qa.get_cue_play_count(K.SUPER_FIRE)==a+1 and qv.spawned_cue_count==v+1,dict(damage=before-hp(p),paid_q=fq.last_contact.paid_q,contacts=fq.contact_count-c,fire_cues=qa.get_cue_play_count(K.SUPER_FIRE)-a,visual_cues=qv.spawned_cue_count-v))
    check(name+' impact completion keeps recoil tail',q.mesh.get_anim_instance().montage_is_playing(qp.super_recoil))
    check(name+' stops loop and uses heavy reaction',qa.active_loop_count==0 and p.mesh.get_anim_instance().get_current_active_montage()==lib.heavy)
    shot_image(name+'-Hit');yield from wait(1.3)
    check(name+' cleans all tails',str(qp.pose_state)=='None' and qv.active_cue_count==0 and qa.active_one_shot_count==0)

def suite():
    global recording
    for n,v in [('t.MaxFPS',60),('JJK.Feedback.HitStop',0),('JJK.Feedback.Reaction',1),('JJK.Feedback.Audio',1),('JJK.Feedback.AudioGain',1),('JJK.Feedback.RangedFX',1),('JJK.Feedback.Camera',1),('JJK.Feedback.CameraStrength',2),('JJK.Feedback.HUD',1)]:cmd(n+' '+str(v))
    for f in [p,q]:
        check(f.get_name()+' one of every configured consumer',all(len(f.get_components_by_class(c))==1 for c in [unreal.CombatFeedbackComponent,unreal.CombatPoseConsumer,unreal.CombatAudioConsumer,unreal.CombatRangedVisualConsumer]))
    check('production profiles preloaded',bool(fd.feedback_profile and lib.light and lib.heavy and lib.guard and pa.audio_profile and pv.visual_profile and pp.guard_start and pp.guard_loop and pp.guard_end and pp.super_recoil))
    check('external ranged outlet enabled on saved profile',fd.feedback_profile.external_ranged_impact)
    check('E native controller modifier and HUD present',bool(pc.combat_camera_feedback and pc.arena_hud))
    unreal.AudioMixerLibrary.start_recording_output(w,120.);recording=True
    report['recording_start_game_seconds']=now();report['recording_start_wall_seconds']=time.monotonic()-started
    yield from melee('prototype-A1','submit_light_attack',35,K.PUNCH_HIT,lib.light)
    yield from melee('prototype-HeavyPunch','submit_heavy_punch',90,K.HEAVY_HIT,lib.heavy)
    yield from super_sample()
    check('three prototypes gate',all(x['passed'] for x in report['checks']))
    assert report['checks'][-1]['passed'], 'Resolve prototype failures before expanding the move set'
    # Expand only after the three actual prototypes have been observed.
    for name,method,n,total,cue in [('PunchChain','submit_light_attack',4,185,K.PUNCH_HIT),('KickChain','submit_kick',3,135,K.KICK_HIT)]:
        reset();yield from wait(.35);mark(name);before=hp(q);c=fp.contact_count;a=pa.get_cue_play_count(cue);heavy=pa.get_cue_play_count(K.HEAVY_HIT)
        for i in range(n):
            getattr(p.get_combat_input(),method)();yield from until(lambda:fp.contact_count>=c+i+1,2)
        check(name+' all segments, unchanged total, ordinary cues plus heavy finisher',fp.contact_count==c+n and hp(q)==before-total and pa.get_cue_play_count(cue)==a+n-1 and pa.get_cue_play_count(K.HEAVY_HIT)==heavy+1,dict(contacts=fp.contact_count-c,damage=before-hp(q),ordinary_sounds=pa.get_cue_play_count(cue)-a,heavy_sounds=pa.get_cue_play_count(K.HEAVY_HIT)-heavy))
        yield from wait(1.5);check(name+' recovers',p.can_act() and q.can_act())
    yield from melee('HeavyKick','submit_heavy_kick',100,K.HEAVY_HIT,lib.heavy)
    reset();yield from wait(.3);mark('Guard-lifecycle');s=qp.guard_start_count;l=qp.guard_loop_count;e=qp.guard_end_count
    q.get_combat_input().notify_guard_pressed();yield from wait(.08)
    check('guard enters B start from real intent',qp.guard_start_count==s+1 and q.mesh.get_anim_instance().montage_is_playing(qp.guard_start))
    yield from wait(1.5)
    check('guard loop wraps without replaying or ending',qp.guard_loop_count==l+1 and q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop))
    c=fp.contact_count;a=pa.get_cue_play_count(K.GUARD);p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>c)
    check('guard contact overrides loop with B impact and D guard',fp.last_contact.result==unreal.CombatFeedbackResult.GUARD and q.mesh.get_anim_instance().get_current_active_montage()==lib.guard and pa.get_cue_play_count(K.GUARD)==a+1)
    shot_image('Guard-impact');yield from wait(.07)
    check('guard loop cannot overwrite active impact/stun',q.mesh.get_anim_instance().get_current_active_montage()==lib.guard and tag(q,'State.GuardStun'))
    yield from until(lambda:q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop))
    check('guard resumes loop only after stun clears',not tag(q,'State.GuardStun') and qp.guard_loop_count==l+2)
    q.get_combat_input().notify_guard_released();yield from wait(.07)
    check('legal release plays B end once',qp.guard_end_count==e+1 and q.mesh.get_anim_instance().montage_is_playing(qp.guard_end))
    yield from wait(.6);check('guard end returns idle',str(qp.pose_state)=='None' and q.can_act())
    reset();yield from wait(.2);q.get_combat_input().notify_guard_pressed();yield from wait(.35)
    e=qp.guard_end_count;q.get_combat_input().notify_guard_released();q.get_combat_input().submit_light_attack();yield
    check('new attack interrupts owned loop without guard-end overwrite',q.is_attacking() and qp.guard_end_count==e and not q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop))
    reset();yield from wait(.2);q.get_combat_input().notify_guard_pressed();yield from wait(.35);cmd('JJK.Feedback.Reaction 0');yield
    check('Reaction off cleans owned guard pose',str(qp.pose_state)=='None' and not q.mesh.get_anim_instance().montage_is_playing(qp.guard_loop));cmd('JJK.Feedback.Reaction 1')
    reset(600);yield from wait(.7);yield from stance(q);mark('MobileBlast');r=qp.recoil_count;c=fq.contact_count;v=qv.spawned_cue_count
    q.get_combat_input().notify_attack_pressed();yield from wait(.4);q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>c)
    check('mobile real Hit has C effect/D audio and no super recoil',fq.contact_count==c+1 and qv.spawned_cue_count==v+1 and qa.last_cue==K.RANGED_HIT and qp.recoil_count==r);shot_image('MobileBlast-hit');yield from wait(1)
    for reason in ['early-release','menu-cancel']:
        reset(600);yield from wait(.7);yield from stance(q);mark(reason);r=qp.recoil_count;fire=fq.fire_count
        q.get_combat_input().notify_kick_pressed();yield from wait(.06)
        if reason=='menu-cancel':gm.set_training_menu_open(True)
        q.get_combat_input().notify_kick_released();yield from wait(1)
        check(reason+' has no Fire/recoil or loop',fq.fire_count==fire and qp.recoil_count==r and qa.active_loop_count==0)
    temp(fd,'initial_energy',100.);reset(600);yield from wait(.7);mark('Domain');c=fp.contact_count;v=pv.spawned_cue_count;r=pp.recoil_count
    p.get_combat_input().notify_domain_pressed();yield from until(lambda:fp.contact_count>c,4)
    check('domain real orb joins C/D with no manual recoil',fp.contact_count>c and fp.last_contact.tier==unreal.CombatFeedbackTier.DOMAIN_ORB and pv.spawned_cue_count>v and pa.last_cue==K.RANGED_HIT and pp.recoil_count==r)
    shot_image('Domain-hit');yield from wait(.8)
    gm.reset_training();yield from wait(.1)
    check('domain reset releases all owned visual/audio tails',pv.active_cue_count==0 and pv.active_orb_trail_count==0 and pa.active_one_shot_count==0)
    for i in range(20):
        reset(600 if i%4 in [1,2] else 120);yield from wait(.2)
        if i%4==0:q.get_combat_input().notify_guard_pressed();yield from wait(.3)
        elif i%4==1:yield from stance(q);q.get_combat_input().notify_kick_pressed();yield from wait(.3)
        elif i%4==2:
            yield from stance(q);q.get_combat_input().notify_kick_pressed();yield from wait(1.35);q.get_combat_input().notify_kick_released();yield from until(lambda:str(qp.pose_state)=='SuperRecoil',2)
            check('reset fixture '+str(i+1)+' actually enters recoil',str(qp.pose_state)=='SuperRecoil')
        else:p.get_combat_input().submit_heavy_punch();yield from wait(.6)
        gm.reset_training();yield from wait(.08)
        check('mixed reset '+str(i+1)+' clears all consumer instances',all(str(x.pose_state)=='None' for x in [pp,qp]) and all(x.active_loop_count==0 and x.active_one_shot_count==0 for x in [pa,qa]) and all(x.active_cue_count==0 and x.active_orb_trail_count==0 for x in [pv,qv]) and not pc.arena_hud.get_feedback_texts())
    # Return to normal resources and repeat the prototypes after the whole loop.
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    mark('post-reset-prototypes')
    yield from melee('post-reset-A1','submit_light_attack',35,K.PUNCH_HIT,lib.light)
    yield from melee('post-reset-HeavyPunch','submit_heavy_punch',90,K.HEAVY_HIT,lib.heavy)
    yield from super_sample('post-reset-SuperBlast')
    check('all frames written',all(Path(x).exists() for x in report['captures']))

gen=suite();started=time.monotonic()
def finish():
    global recording
    unreal.unregister_slate_post_tick_callback(handle)
    if recording:
        unreal.AudioMixerLibrary.stop_recording_output(w,unreal.AudioRecordingExportType.WAV_FILE,'A2-integrated-combat',str(out));recording=False
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    gm.set_training_menu_open(False);gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;report['recording_end_game_seconds']=now();write()
def tick(dt):
    try:
        sample()
        if time.monotonic()-started>280:raise TimeoutError('integration suite')
        next(gen)
    except StopIteration:report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
