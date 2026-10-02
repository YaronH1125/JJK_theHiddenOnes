"""E real inputs/results and HUD/camera inspection. E_run owns PIE and cleanup.

CONTRACT checks replay a copied real snapshot only in E's local consumer; they
never publish combat events or inflict damage. Shot tests use actual input,
release and spawned projectile, with a labelled local pulse stress fixture.
"""
import json
import math
from pathlib import Path
import time
import traceback
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter();fd=p.get_definition()
fp,fq=p.get_combat_feedback(),q.get_combat_feedback()
cam=pc.combat_camera_feedback;hud=pc.arena_hud
out=Path(unreal.Paths.project_saved_dir())/'FeedbackE'
archive=Path(json.loads((out/'latest-run.json').read_text(encoding='utf-8'))['path'])
old={n:fd.get_editor_property(n) for n in ['initial_health','initial_cursed_energy','initial_energy']}
old_guard=fd.guard_config.copy()
settings,mode=gm.settings.copy(),gm.opponent_mode
report={'status':'running','engine':unreal.SystemLibrary.get_engine_version(),'map':w.get_path_name(),
        'checks':[],'samples':[],'screenshots':[],
        'human_comfort':'not performed','final_A_integration':'pending',
        'fixture':'D audio consumer temporarily attached, not saved. HitStop off. Native E PC/HUD binding.',
        'input_note':'Shared CombatInput requests drive actual abilities; right mouse uses the native simulated InputKey gateway. No physical human input is claimed.',
        'lifecycle_fixture_note':'JJKDebugKill explicitly injects the native death queue for lifecycle/match cleanup checks; not evidence of real hit damage or a human match.',
        'contract_note':'CONTRACT rows are local consumer snapshot replay, not gameplay hit evidence.'}
def write():(archive/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def check(n,b,d=None):report['checks'].append({'name':n,'passed':bool(b),'detail':d});write()
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s,pc)
def hp(f):return f.get_fighter_attribute_set().health.current_value
def state():return json.loads(hud.get_feedback_state())
def vec(v):return [v.x,v.y,v.z]
def rot(r):return [r.pitch,r.yaw,r.roll]
def distance(a,b):return sum((x-y)**2 for x,y in zip(a,b))**.5
def forward(r):
    pitch,yaw=math.radians(r.pitch),math.radians(r.yaw)
    return [math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch)]
def reset(gap=120,preserve_mesh=False):
    # Automated Editor runs are in the background; explicitly model a reactivated game for each case.
    pc.handle_app_activation_changed(True)
    pc.set_training_panel_open(False)
    gm.set_training_menu_open(False);gm.set_training_settings(unreal.TrainingSettings());gm.set_opponent_mode(unreal.OpponentMode.STATIC)
    gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0))
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.set_component_tick_enabled(True);f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
        if not preserve_mesh:f.mesh.set_editor_property('bPauseAnims',False)
def stance(f):f.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
def capture(name):
    file=archive/(name+'.png');cmd('Shot showui -nosuffix filename='+file.as_posix())
    yield from wait(.25)
    check(name+' screenshot exists',file.exists(),str(file));report['screenshots'].append(str(file));write()
    if file.exists():
        png=file.read_bytes();report.setdefault('screenshot_dimensions',{})[name]=[int.from_bytes(png[16:20],'big'),int.from_bytes(png[20:24],'big')];write()
        report['game_viewport_size']=list(pc.get_viewport_size());write()
def sample_roll(seconds=.2):
    start=now();samples=[]
    while now()-start<seconds:
        samples.append({'time':now()-start,'roll':cam.applied_roll,'control':rot(pc.get_control_rotation())})
        yield
    return samples
def melee(label,source=p,method='submit_heavy_punch',guard=False):
    reset();yield from wait(.4)
    if guard=='chip':
        cfg=fd.guard_config.copy();cfg.set_editor_property('chip_damage',True);fd.set_editor_property('guard_config',cfg)
    target=q if source==p else p;feedback=source.get_combat_feedback()
    if guard:target.get_combat_input().notify_guard_pressed()
    before=hp(target);count=feedback.contact_count;started=cam.started_count
    control=rot(pc.get_control_rotation())
    getattr(source.get_combat_input(),method)()
    yield from until(lambda:feedback.contact_count>count,2)
    snap=feedback.last_contact.copy()
    texts=list(hud.get_feedback_texts());authored_peak=cam.peak_roll;samples=yield from sample_roll()
    row={'case':label,'contact_delta':feedback.contact_count-count,'damage':before-hp(target),
         'tier':str(snap.tier),'result':str(snap.result),'max_roll':max([abs(x['roll']) for x in samples] or [0]),
         'samples':samples,'authored_peak':authored_peak,'texts_at_contact':texts,'starts':cam.started_count-started,'hud':state()}
    report['samples'].append(row);write()
    check(label+' one real contact',feedback.contact_count==count+1,row['result'])
    check(label+' does not change ControlRotation',all(distance(x['control'],control)<.001 for x in samples))
    check(label+' exact zero after envelope',abs(cam.applied_roll)<.00001)
    if guard:
        check(label+' chip is Guard and actual damage',snap.result==unreal.CombatFeedbackResult.GUARD and abs(snap.actual_damage-(before-hp(target)))<.001)
        check(label+' HUD shows Guard with actual chip if present',any('防御' in t and ('·' in t if snap.actual_damage>.01 else True) for t in texts),texts)
    else:check(label+' HUD uses actual contact result',snap.result==unreal.CombatFeedbackResult.HIT and bool(texts),texts)
    yield from wait(.9)
    fd.set_editor_property('guard_config',old_guard)
    return row,snap

def actual_shot(level,aim,seed):
    reset(600,preserve_mesh=True);yield from wait(.5);yield from stance(p)
    # Keep this target and entire actual ray/locked direction identical across all modes.
    p.character_movement.set_component_tick_enabled(False);q.character_movement.set_component_tick_enabled(False)
    p.set_actor_location(unreal.Vector(0,0,5000),False,True);q.set_actor_location(unreal.Vector(600,0,5000),False,True)
    pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0));p.set_aim_intent(aim);yield from wait(.8)
    cmd('JJK.Feedback.CameraStrength '+str(level));p.get_combat_input().notify_attack_pressed();yield from wait(.4)
    # Local-only pulse from a real heavy snapshot while holding. No gameplay settlement or damage.
    e=seed.copy();e.set_editor_property('round_id',fp.last_action.round_id)
    e.set_editor_property('source_generation',fp.get_generation());e.set_editor_property('target_generation',fq.get_generation())
    e.set_editor_property('attack_instance_id',800000+level+(10 if aim else 0));pc.debug_consume_feedback_contact(e)
    yield from wait(.04)
    loc,r=pc.get_combat_aim_view_point();render=pc.player_camera_manager.get_camera_rotation()
    direction=forward(r);ray=[loc.x+direction[0]*fd.mobile_blast.range,loc.y+direction[1]*fd.mobile_blast.range,loc.z+direction[2]*fd.mobile_blast.range]
    row={'level':level,'aim':aim,'base_location':vec(loc),'base_rotation':rot(r),'render_rotation':rot(render),
         'render_roll':cam.applied_roll,'ray_endpoint':ray,'control':rot(pc.get_control_rotation()),
         'pulse_fixture':'local consumer replay; shot below uses actual input/Fire/projectile'}
    before=fp.fire_count;p.get_combat_input().notify_attack_released()
    yield from until(lambda:fp.fire_count>before,2)
    projectiles=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.BlastProjectile) if a.get_owner()==p]
    check('E-T02 shot '+str(level)+' aim '+str(aim)+' actual Fire/projectile',fp.fire_count==before+1 and len(projectiles)==1,len(projectiles))
    if projectiles:
        proj=projectiles[0];row['projectile_direction']=forward(proj.get_actor_rotation());row['spawn_muzzle']=vec(fp.last_action.muzzle.translation)
        d=[ray[i]-row['spawn_muzzle'][i] for i in range(3)];length=sum(x*x for x in d)**.5;expected=[x/length for x in d]
        check('E-T02 locked ray equals actual projectile '+str(level)+' aim '+str(aim),distance(expected,row['projectile_direction'])<.0001,{'expected':expected,'actual':row['projectile_direction']})
    check('E-T02 rendered roll preserves base forward '+str(level)+' aim '+str(aim),distance(forward(render),forward(r))<.000001)
    if aim:check('E-T03 right aim completely suppresses roll '+str(level),abs(row['render_roll'])<.00001)
    report['samples'].append({'case':'aim-shot',**row});write()
    yield from wait(.8)
    p.set_aim_intent(False);p.character_movement.set_component_tick_enabled(True);q.character_movement.set_component_tick_enabled(True)
    return row

def edges():
    reset();yield from wait(.4)
    check('E-T06 fresh PIE binds once',pc.get_feedback_binding_count()==2 and bool(cam))
    # The existing right mouse EnhancedInput binding is exercised through the native input gateway.
    yield from stance(p);pc.debug_send_key('RightMouseButton',True);yield from wait(.2)
    check('E-T03 real right mouse input enables ranged aim',p.is_aim_intent() and p.is_aiming_effective())
    pc.debug_send_key('RightMouseButton',False);yield from wait(.3)
    check('E-T03 real right mouse release restores ranged idle',not p.is_aim_intent() and not p.is_aiming_effective())
    reset();yield from wait(.4);cmd('JJK.Feedback.CameraStrength 2');c=fp.contact_count
    p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>c,2)
    want=unreal.Rotator(pitch=12,yaw=37,roll=0);pc.set_control_rotation(want);yield from wait(.035)
    base_loc,base_rot=pc.get_combat_aim_view_point();render=pc.player_camera_manager.get_camera_rotation()
    check('E-T03 free look during actual heavy pulse stays player controlled',distance(rot(pc.get_control_rotation()),rot(want))<.001 and abs(cam.applied_roll)>.05,
          {'control':rot(pc.get_control_rotation()),'render':rot(render),'base':rot(base_rot)})
    yield from wait(.25)
    check('E-T03 no auto recenter after melee pulse',distance(rot(pc.get_control_rotation()),rot(want))<.001 and cam.applied_roll==0)
    yield from wait(.9);yield from stance(p);pc.debug_send_key('RightMouseButton',True);yield from wait(.1)
    p.get_combat_input().notify_attack_pressed();yield from wait(.25)
    check('E-T03 aim remains effective during real held cannon',p.is_aiming_effective() and state()['charging'],
          {'aim_intent':p.is_aim_intent(),'aim_effective':p.is_aiming_effective(),'hud':state()})
    p.get_combat_input().notify_stance_switch_pressed();yield from wait(.2)
    check('E-T03 shape switch cancels charge without reclaiming camera yaw',not p.is_aiming_effective() and not state()['charging'] and distance(rot(pc.get_control_rotation()),rot(want))<.001,
          {'control':rot(pc.get_control_rotation()),'hud':state()})
    pc.debug_send_key('RightMouseButton',False)
    reset(600);yield from wait(.5);yield from stance(p)
    fire=fp.fire_count;p.get_combat_input().notify_kick_pressed();yield from wait(.2)
    check('E-T05 short super hold shows not ready without false Full',state()['charging'] and not state()['gate_ready'] and not state()['full'],state())
    yield from capture('08-v9-super-gate')
    p.get_combat_input().notify_kick_released();yield from wait(.5)
    check('E-T05 below-gate release is rejected with real fuse and no Fire',not state()['charging'] and state()['super_cooldown'] and fp.fire_count==fire,state())
    reset();yield from wait(.3);pc.set_training_panel_open(True);gm.set_training_menu_open(False);yield from wait(.04)
    check('E-T06 visible menu still suppresses feedback if GM already closed it',pc.training_panel.is_in_viewport() and not pc.can_play_combat_feedback())
    pc.set_training_panel_open(False);yield from wait(.04)
    panel_state={'panel_present':pc.training_panel.is_in_viewport(),'gm_menu':gm.is_training_menu_open(),'can_play_before_activation':pc.can_play_combat_feedback()}
    pc.handle_app_activation_changed(True)
    panel_state['can_play_after_activation']=pc.can_play_combat_feedback()
    check('E-T06 idempotent close removes stale visible training panel',not panel_state['panel_present'] and not panel_state['gm_menu'] and panel_state['can_play_after_activation'],panel_state)
    pc.set_training_panel_open(True);gm.set_training_menu_open(False);pc.toggle_training_panel();yield from wait(.04)
    panel_state={'panel_present':pc.training_panel.is_in_viewport(),'gm_menu':gm.is_training_menu_open(),'can_play_before_activation':pc.can_play_combat_feedback()}
    pc.handle_app_activation_changed(True)
    panel_state['can_play_after_activation']=pc.can_play_combat_feedback()
    check('E-T06 F1 toggle closes the visible panel after state desync',not panel_state['panel_present'] and panel_state['can_play_after_activation'],panel_state)
    for mode_name in ['menu','focus','input-disable','reset','death']:
        reset();yield from wait(.4);c=fp.contact_count;p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>c,2)
        if mode_name=='menu':pc.set_training_panel_open(True)
        elif mode_name=='focus':pc.handle_app_activation_changed(False)
        elif mode_name=='input-disable':pc.set_combat_input_enabled(False)
        elif mode_name=='death':p.jjk_debug_kill();yield from until(lambda:p.is_dead(),1)
        else:gm.reset_training()
        yield from wait(.06)
        if mode_name=='menu':pc.set_training_panel_open(False)
        if mode_name=='focus':pc.handle_app_activation_changed(True)
        pc.set_combat_input_enabled(True)
    for i in range(20):reset();yield from wait(.04)
    cmd('JJK.Feedback.CameraStrength 0');reset();yield from wait(.4)
    audio=p.get_component_by_class(unreal.CombatAudioConsumer);played=audio.played_cue_count;c=fp.contact_count;prompts=hud.contact_prompt_count;local=pc.feedback_contact_count
    p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>c,2)
    detail={'can_play':pc.can_play_combat_feedback(),'binding_count':pc.get_feedback_binding_count(),'contacts_seen_delta':pc.feedback_contact_count-local,
            'damage':fp.last_contact.actual_damage,'source_gen_event':fp.last_contact.source_generation,'source_gen_now':fp.get_generation(),
            'target_gen_event':fp.last_contact.target_generation,'target_gen_now':fq.get_generation(),
            'source_current':fp.is_current(fp.last_contact.round_id,fp.last_contact.source_generation),
            'target_current':fq.is_current(fp.last_contact.round_id,fp.last_contact.target_generation),
            'hud_enabled':fp.is_channel_enabled(unreal.CombatFeedbackChannel.HUD),'prompts_delta':hud.contact_prompt_count-prompts,
            'p_dead':p.is_dead(),'q_dead':q.is_dead(),'menu':gm.is_training_menu_open(),'resolved':gm.is_match_resolved(),
            'audio_delta':audio.played_cue_count-played,'texts':list(hud.get_feedback_texts())}
    check('E-T06/07 next real hit after focus/death/20 reset keeps camera off audio and HUD',cam.applied_roll==0 and detail['audio_delta']>0 and detail['prompts_delta']==1,detail)
    fd.set_editor_property('initial_energy',100.);reset(600);yield from wait(.6)
    p.get_combat_input().notify_domain_pressed();q.get_combat_input().notify_domain_pressed();yield from wait(1.3)
    check('E-T06 restarted domain capture has no stale training panel',not pc.training_panel or not pc.training_panel.is_in_viewport())
    check('E-T05 simultaneous true domains show world suppression',state()['domain_suppressed'] and gm.get_domain_status_for(p).suppressed,
          {'hud':state(),'p_active':gm.get_domain_status_for(p).active,'q_active':gm.get_domain_status_for(q).active})
    fd.set_editor_property('initial_energy',old['initial_energy'])

def reentry():
    global q,fq
    reset();yield from wait(.5)
    check('E-T06 new PIE has unique sources/native modifier',pc.get_feedback_binding_count()==2 and bool(cam))
    cmd('JJK.Feedback.CameraStrength 2');row,stale=yield from melee('E-T06 new PIE fresh heavy')
    check('E-T06 new PIE exactly one pulse and prompt',row['starts']==1 and row['max_roll']>.1)
    pc.un_possess();yield from wait(.06)
    check('E-T06 unpossess unbinds both sources and clears display',pc.get_feedback_binding_count()==0 and cam.applied_roll==0 and state()['floats']==0)
    pc.possess(p);yield from wait(.06)
    check('E-T06 repossess binds once and keeps one audio component',pc.get_feedback_binding_count()==2 and len(p.get_components_by_class(unreal.CombatAudioConsumer))==1)
    old_q=q;q.destroy_actor();yield from until(lambda:gm.get_opponent_fighter() is not None and gm.get_opponent_fighter()!=old_q,3)
    q=gm.get_opponent_fighter();fq=q.get_combat_feedback();yield from wait(.1)
    check('E-T06 destroyed fighter replaced without stale source binding',q!=old_q and pc.get_feedback_binding_count()==2)
    before=pc.feedback_contact_count;pc.debug_consume_feedback_contact(stale)
    check('CONTRACT destroyed target snapshot cannot create HUD/camera',pc.feedback_contact_count==before and state()['floats']==0 and cam.applied_roll==0)
    row,_=yield from melee('E-T06 replacement fighter real heavy')
    check('E-T06 replacement receives exactly one new pulse',row['starts']==1 and row['damage']==90)
    reset(600);gm.set_opponent_mode(unreal.OpponentMode.AI);yield from wait(.2)
    q.jjk_debug_kill();yield from until(lambda:gm.is_match_resolved(),2)
    check('E-T06 actual match outcome clears feedback',gm.is_match_resolved() and cam.applied_roll==0 and state()['floats']==0)
    gm.restart_match();gm.set_opponent_mode(unreal.OpponentMode.STATIC);yield from wait(.3)
    check('E-T06 match restart has fresh clear state and unique bindings',not gm.is_match_resolved() and state()['floats']==0 and pc.get_feedback_binding_count()==2)
    fd.set_editor_property('initial_energy',100.);reset(600);yield from wait(.6)
    p.get_combat_input().notify_domain_pressed();q.get_combat_input().notify_domain_pressed();yield from wait(1.3)
    check('E-T06 match restart leaves no panel over domain screenshot',not pc.training_panel or not pc.training_panel.is_in_viewport())
    yield from capture('07-v9-domain-short-banner')
    fd.set_editor_property('initial_energy',old['initial_energy'])

def suite():
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.Camera 1');cmd('JJK.Feedback.HUD 1');cmd('JJK.Feedback.Audio 1')
    if globals().get('R1_EXACT_VIEWPORT',False):
        cmd('r.SetRes 1920x1080wf');yield from wait(.5)
    if globals().get('E_EDGE_ONLY',False):
        yield from edges()
        return
    if globals().get('E_REENTRY_ONLY',False):
        yield from reentry()
        return
    check('E native modifier and two unique source subscriptions',bool(cam) and pc.get_feedback_binding_count()==2)
    check('V9 all 12 hard referenced textures exist',all(unreal.load_asset('/Game/UI/HUD/V9/T_'+n) for n in
          ['scroll','portrait','punch','heavy','kick','hkick','swap','blast','sblast','aim','domain','mouse']))
    check('V9 actual game viewport is 1920x1080',list(pc.get_viewport_size())==[1920,1080],list(pc.get_viewport_size()))
    reset();yield from wait(.6);yield from capture('01-v9-melee')
    amplitudes=[];peak_bounds=[];heavy=None
    for level in [0,1,2]:
        cmd('JJK.Feedback.CameraStrength '+str(level));row,heavy=yield from melee('E-T01 heavy level '+str(level))
        amplitudes.append(row['max_roll'])
        peak_bounds.append(row['authored_peak'])
        check('E-T01 heavy damage unchanged level '+str(level),abs(row['damage']-90)<.001)
    # Different rendered frames can miss a short pulse's peak by different
    # amounts. Check real visible growth plus the configured half/full bounds;
    # don't compare unrelated frame samples as an exact amplitude ratio.
    check('E-T01 0/0.5/1 increasing strength',amplitudes[0]==0 and
          0<amplitudes[1]<=.3001 and amplitudes[1]<amplitudes[2]<=.6001 and
          abs(peak_bounds[1]-.6)<.0001 and abs(peak_bounds[2]-.6)<.0001,
          {'rendered_peaks':amplitudes,'authored_peaks':peak_bounds,'fixture':'independent real frame samples, not matched pulse phases'})
    cmd('JJKCameraStrength 2');check('E-T01 controller Exec setting entry',pc.get_camera_feedback_strength()==2)
    row,light=yield from melee('E-T01 A1 default zero',method='submit_light_attack')
    check('E-T01 A1 has no outgoing pulse',row['max_roll']==0 and row['starts']==0)
    row,_=yield from melee('E-T04 player receives heavy',source=q)
    check('E-T04 actual victim pulse survives victim input cleanup',row['max_roll']>.25 and row['max_roll']<=.60,row['max_roll'])
    row,_=yield from melee('E-T04 player Guard',source=q,guard=True)
    check('E-T04 guard is weaker than received heavy',0<row['max_roll']<.1,row['max_roll'])
    row,_=yield from melee('E-T05 Guard chip enabled temporary fixture',source=q,guard='chip')
    check('E-T05 Guard chip uses actual damage and blood trail',row['damage']>0 and state()['player_health']==hp(p)
          and row['hud']['player_ghost']>row['hud']['player_hp_fill'],row['hud'])

    # Real combo contacts drive the finisher mapping; no synthetic gameplay notifications.
    for method,segments in [('submit_light_attack',4),('submit_kick',3)]:
        reset();yield from wait(.4);starts=cam.started_count;seen=[]
        for i in range(segments):
            before=fp.contact_count;getattr(p.get_combat_input(),method)();yield from until(lambda:fp.contact_count>before,2)
            # Queue on the confirmed contact, matching the legal real combo
            # buffer. An arbitrary extra delay may miss that window at low FPS.
            seen.append(str(fp.last_contact.tier))
        check('E-T01 '+method+' final segment uses Finisher',fp.last_contact.tier==unreal.CombatFeedbackTier.FINISHER,seen)
        check('E-T01 '+method+' only finisher adds bounded pulse',cam.started_count==starts+1 and .149<=cam.peak_roll<=.151,{'starts':cam.started_count-starts,'peak':cam.peak_roll})

    # Freeze one existing skeletal pose across modes so changing muzzle animation is not a test confound.
    p.mesh.set_editor_property('bPauseAnims',True)
    for aim in [False,True]:
        rows=[]
        for level in [0,1,2]:rows.append((yield from actual_shot(level,aim,heavy)))
        check('E-T02 same target ray independent of strength aim '+str(aim),all(distance(rows[0]['ray_endpoint'],r['ray_endpoint'])<.01 for r in rows[1:]),rows)
        check('E-T02 actual projectile direction independent of strength aim '+str(aim),all(distance(rows[0]['projectile_direction'],r['projectile_direction'])<.001 for r in rows[1:]),[r['projectile_direction'] for r in rows])
        if not aim:check('E-T02 lock captured during visible render roll',abs(rows[2]['render_roll'])>.15,rows[2]['render_roll'])
    p.mesh.set_editor_property('bPauseAnims',False)

    # CONTRACT single blend exit, dedup, priority, hard end. All copies are consumer-only.
    reset();yield from wait(.4);cmd('JJK.Feedback.CameraStrength 2');before=fp.contact_count
    p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>before,2)
    real=fp.last_contact.copy();count=pc.feedback_contact_count;started=cam.started_count
    pc.debug_consume_feedback_contact(real)
    check('CONTRACT duplicate contact cannot start a second pulse/prompt',pc.feedback_contact_count==count and cam.started_count==started)
    yield from wait(.3)
    e=real.copy();e.set_editor_property('attack_instance_id',900001);pc.debug_consume_feedback_contact(e)
    for i in range(10):e.set_editor_property('attack_instance_id',900002+i);pc.debug_consume_feedback_contact(e)
    check('CONTRACT repeated pulses merge without addition',cam.peak_roll<=.6001 and cam.merged_count>=10,{'peak':cam.peak_roll,'merged':cam.merged_count})
    victim=e.copy();victim.set_editor_property('source',q);victim.set_editor_property('target',p)
    victim.set_editor_property('source_generation',fq.get_generation());victim.set_editor_property('target_generation',fp.get_generation())
    victim.set_editor_property('attack_instance_id',910000);pc.debug_consume_feedback_contact(victim)
    check('CONTRACT player damage preempts own hit',cam.pulse_priority==3 and abs(cam.peak_roll-.55)<.0001,{'priority':cam.pulse_priority,'peak':cam.peak_roll})
    e.set_editor_property('attack_instance_id',900100);dropped=cam.dropped_count;pc.debug_consume_feedback_contact(e)
    check('CONTRACT outgoing cannot override player damage',cam.pulse_priority==3 and cam.dropped_count==dropped+1)
    yield from wait(.18);check('CONTRACT mixture ends within 150ms',abs(cam.applied_roll)<.00001)
    check('CONTRACT consumer replay never changes A LastContact',fp.last_contact.attack_instance_id<800000)
    yield from wait(.8);check('E-T04 expired prompts removed',state()['floats']==0)

    # Real attributes: infinite life does not turn resolved damage into invented blood loss.
    fd.set_editor_property('initial_health',20.);reset();fd.set_editor_property('initial_health',old['initial_health'])
    s=unreal.TrainingSettings();s.set_editor_property('infinite_health',True);gm.set_training_settings(s);yield from wait(.4)
    before=hp(q);contact=fp.contact_count;p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>contact,2)
    check('E-T05 infinite life clamps at one instead of zero damage',hp(q)==1 and fp.last_contact.actual_damage==before-1,
          {'before':before,'after':hp(q),'actual_damage':fp.last_contact.actual_damage})
    yield from wait(1);before=hp(q);contact=fp.contact_count;p.get_combat_input().submit_light_attack();yield from until(lambda:fp.contact_count>contact,2)
    yield from wait(.03)
    check('E-T05 infinite life HUD shows unchanged actual health',hp(q)==before and state()['opponent_health']==before and fp.last_contact.actual_damage==0,state())
    check('E-T05 infinite life still confirms real Hit',list(hud.get_feedback_texts())==['命中'],list(hud.get_feedback_texts()))
    yield from capture('02-v9-infinite-hit')

    for kind in ['mobile','super']:
        reset(600);yield from wait(.5);yield from stance(p)
        inp=p.get_combat_input();(inp.notify_kick_pressed if kind=='super' else inp.notify_attack_pressed)()
        end=now()+1.0;normal=[]
        while now()<end:normal.append(state());yield
        check('E-T05 '+kind+' 20Hz payment never fakes resource limit',all(not x['limited'] for x in normal))
        yield from wait(2.6)
        v=state();check('E-T05 '+kind+' true PaidQ full',v['full'] and not v['limited'] and abs(v['paid_q']-1)<.001,v)
        expected=fd.super_blast.max_cost if kind=='super' else fd.mobile_blast.max_cost
        check('E-T05 '+kind+' paid cost ignores remaining energy',abs(v['paid_cost']-expected)<.01,v)
        yield from capture('03-v9-'+kind+'-full')
        # A already tested 60s holds; E verifies UI remains unchanged over an additional short hold.
        yield from wait(2);check('E-T05 '+kind+' full stays stable until release',state()['full'] and state()['paid_q']==v['paid_q'])
        if kind=='super':
            inp.notify_kick_released();yield from until(lambda:state()['super_cooldown'],3)
            check('E-T05 successful super starts actual cooldown',state()['super_cooldown'],state())
            yield from capture('04-v9-super-cooldown')
            before=state()['cooldown_remaining'];pc.set_training_panel_open(True);yield from wait(.4);pc.set_training_panel_open(False);yield from wait(.1)
            check('E-T06 menu preserves elapsed cooldown',state()['super_cooldown'] and state()['cooldown_remaining']<before-.3,state())
        for cost in [10. if kind=='mobile' else 55.]:
            fd.set_editor_property('initial_cursed_energy',cost);reset(600);yield from wait(.5);yield from stance(p)
            (inp.notify_kick_pressed if kind=='super' else inp.notify_attack_pressed)();yield from wait(3.6)
            v=state();check('E-T05 '+kind+' limited never reports Full',v['limited'] and not v['full'] and 0<v['paid_q']<1,v)
            check('E-T05 '+kind+' preview reads PaidQ',abs(v['paid_q']-fp.last_action.paid_q)<.001,v)
            yield from capture('05-v9-'+kind+'-limited')
            fd.set_editor_property('initial_cursed_energy',old['initial_cursed_energy'])
    reset(600);yield from wait(.5);yield from stance(p);p.get_combat_input().notify_kick_pressed();yield from wait(.2)
    p.get_combat_input().notify_kick_released();yield from wait(.5)
    check('E-T05 paid short super rejection still has real fuse',state()['super_cooldown'] and not state()['charging'],state())

    # Domain contact is Hit under current unguardable rules, and scheduling state is read directly.
    fd.set_editor_property('initial_energy',100.);reset(600);yield from wait(.6)
    q.get_combat_input().notify_guard_pressed();before=fp.contact_count;p.get_combat_input().notify_domain_pressed()
    yield from until(lambda:fp.contact_count>before,4)
    check('E-T05 domain is actual Hit even with guarding victim',fp.last_contact.tier==unreal.CombatFeedbackTier.DOMAIN_ORB and fp.last_contact.result==unreal.CombatFeedbackResult.HIT,
          {'tier':str(fp.last_contact.tier),'result':str(fp.last_contact.result),'texts':list(hud.get_feedback_texts())})
    check('E-T04 outgoing domain does not roll each ball',cam.applied_roll==0)
    check('E-T05 HUD reads current domain session',state()['domain']==gm.get_domain_status_for(p).active,state())
    reset(600);yield from wait(.6);p.get_combat_input().notify_domain_pressed();q.get_combat_input().notify_domain_pressed();yield from wait(1.3)
    check('E-T05 suppression reads actual world session',state()['domain_suppressed']==gm.get_domain_status_for(p).suppressed and state()['domain_suppressed'],state())
    yield from capture('06-v9-domain-suppressed');fd.set_editor_property('initial_energy',old['initial_energy'])

    for mode in ['menu','focus','input-disable','reset','death']:
        reset();yield from wait(.4);c=fp.contact_count;p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>c,2)
        if mode=='menu':pc.set_training_panel_open(True)
        elif mode=='focus':pc.handle_app_activation_changed(False)
        elif mode=='input-disable':pc.set_combat_input_enabled(False)
        elif mode=='death':p.jjk_debug_kill();yield from until(lambda:p.is_dead(),1)
        else:gm.reset_training()
        yield from wait(.06)
        check('E-T06 '+mode+' clears camera and prompts',abs(cam.applied_roll)<.00001 and state()['floats']==0,{'roll':cam.applied_roll,'hud':state()})
        if mode=='menu':pc.set_training_panel_open(False)
        if mode=='focus':pc.handle_app_activation_changed(True)
        pc.set_combat_input_enabled(True);yield from wait(.2)
        check('E-T06 '+mode+' resumes without replaying stale prompts',state()['floats']==0)
    for i in range(20):
        reset();yield from wait(.035)
        stale=heavy.copy();count=pc.feedback_contact_count;pc.debug_consume_feedback_contact(stale);yield from wait(.02)
        check('E-T06 reset '+str(i+1)+' unique bindings/no stale round',pc.get_feedback_binding_count()==2 and pc.feedback_contact_count==count and cam.applied_roll==0 and state()['floats']==0)

    cmd('JJK.Feedback.CameraStrength 0');reset();yield from wait(.4)
    audio=p.get_component_by_class(unreal.CombatAudioConsumer);played=audio.played_cue_count;c=fp.contact_count;prompts=hud.contact_prompt_count
    p.get_combat_input().submit_heavy_punch();yield from until(lambda:fp.contact_count>c,2)
    check('E-T07 camera off retains real damage, audio and HUD',cam.applied_roll==0 and fp.last_contact.actual_damage==90 and audio.played_cue_count>played and hud.contact_prompt_count>prompts,
          {'damage':fp.last_contact.actual_damage,'audio_delta':audio.played_cue_count-played,'texts':list(hud.get_feedback_texts())})
    yield from wait(.9)
    report['human_comfort']='关/低/标准舒适度、有声真人试玩待用户/F；本报告不代填主观结论'

gen=suite();start=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for n,v in old.items():fd.set_editor_property(n,v)
    fd.set_editor_property('guard_config',old_guard)
    pc.handle_app_activation_changed(True);pc.set_training_panel_open(False)
    for f in [gm.get_player_fighter(),gm.get_opponent_fighter()]:
        if f:f.character_movement.set_component_tick_enabled(True);f.mesh.set_editor_property('bPauseAnims',False)
    gm.set_training_settings(settings);gm.set_opponent_mode(mode);gm.jjk_clear_blockers();gm.reset_training()
    report['seconds']=time.monotonic()-start;write()
def tick(dt):
    try:
        if time.monotonic()-start>270:raise TimeoutError('E callback')
        next(gen)
    except StopIteration:
        report['status']='passed' if all(x['passed'] for x in report['checks']) else 'failed'
        try:finish()
        except Exception:report['status']='failed';report['restore_error']=traceback.format_exc();write()
    except Exception:
        report['status']='failed';report['error']=traceback.format_exc()
        try:finish()
        except Exception:report['restore_error']=traceback.format_exc();write()
write();handle=unreal.register_slate_post_tick_callback(tick)
