"""B isolated real-input/sweep tests; use B_run.py, not directly.

Temporarily assigns the reaction library in memory and restores it. Guard loop
and SuperRecoil previews are explicitly manual, pending A's production wiring.
All rotations use named arguments: Rotator's positional order is roll,pitch,yaw.
"""
import json
import math
from pathlib import Path
import time
import traceback
import unreal

out=Path(json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackB/latest-run.json').read_text())['path'])
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0);gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fd=p.get_definition();inp=p.get_combat_input();hit=p.get_combat_hit();fp=p.get_combat_feedback()
lib=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/DA_FB_Reaction_Ishigori')
old_library=fd.reaction_library
old_cvars={n:unreal.SystemLibrary.get_console_variable_int_value(n) for n in ['t.MaxFPS','JJK.Feedback.HitStop','JJK.Feedback.Reaction']}
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground')
(out/'restore-settings.json').write_text(json.dumps(dict(throttle=old_throttle,cvars=old_cvars,library=old_library.get_path_name() if old_library else None)),encoding='utf-8')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
boom=p.get_component_by_class(unreal.SpringArmComponent)
camera=p.get_component_by_class(unreal.CameraComponent)
camera.set_field_of_view(55.)
boom.set_editor_property('target_arm_length',470.)
boom.set_editor_property('target_offset',unreal.Vector(60,0,0))
boom.set_editor_property('do_collision_test',False) # isolated inspection camera: avoid fighter-driven zoom
report={'status':'running','scope':'temporary library, real combat input and sweeps; not saved integration','checks':[],'contacts':[],'frames':[]}
report['motion_captures']={}
original=[];record=False;label='';last_count=fp.contact_count
def write(): (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def check(n,ok,d=None):report['checks'].append(dict(name=n,passed=bool(ok),detail=d));write()
def v(x):return [round(x.x,3),round(x.y,3),round(x.z,3)]
def distance(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
def now():return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end:yield
def until(fn,t=3):
    end=now()+t
    while not fn() and now()<end:yield
def hp(f):return f.get_fighter_attribute_set().health.current_value
def da(n):return unreal.load_asset('/Game/Training/DA_M3_'+n)
def tag(f,n):
    t=unreal.GameplayTag();t.import_text('(TagName="'+n+'")');return f.has_combat_tag(t)
def temp(o,k,value):
    if not any(a==o and b==k for a,b,c in original):original.append((o,k,o.get_editor_property(k)))
    o.set_editor_property(k,value)
def cmd(s):unreal.SystemLibrary.execute_console_command(w,s)
def reset(gap=120):
    gm.reset_training();pc.set_combat_input_enabled(True)
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def set_camera(view):
    pc.set_control_rotation(unreal.Rotator(pitch=-9.,yaw=90. if view=='side' else 140.,roll=0.))
def snap(name):
    unreal.GameplayStatics.set_game_paused(w,True)
    cmd('HighResShot 1280x720 filename="'+str(out/(name+'.png')).replace('\\','/')+'"')
    deadline=time.monotonic()+20
    while not (out/(name+'.png')).exists() and time.monotonic()<deadline:yield
    check(name+' captured',(out/(name+'.png')).exists())
    unreal.GameplayStatics.set_game_paused(w,False)

def motion(name,duration):
    folder=out/name;folder.mkdir()
    frames=[];end=now()+duration
    while now()<end:
        image=folder/(str(len(frames)).zfill(4)+'.png')
        cmd('Shot -nosuffix filename='+image.as_posix())
        frames.append(dict(file=image.as_posix(),t=now()))
        yield from wait(.045)
    report['motion_captures'][name]=frames
    check(name+' frame sequence',len(frames)>3 and all(Path(f['file']).exists() for f in frames[:-1]),len(frames))

def sample():
    global last_count
    if not record:return
    m=p.mesh.get_anim_instance().get_current_active_montage()
    report['frames'].append(dict(case=label,t=now(),segment=hit.get_segment_id(),
        montage=m.get_name() if m else None,position=p.mesh.get_anim_instance().montage_get_position(m) if m else 0,
        p=v(p.get_actor_location()),q=v(q.get_actor_location()),
        bones={b:v(p.mesh.get_socket_location(b)) for b in ['hand_l','hand_r','foot_l','foot_r']},
        target_bones={b:v(q.mesh.get_socket_location(b)) for b in ['spine_03','head','pelvis']}))
    if fp.contact_count!=last_count:
        last_count=fp.contact_count;c=fp.last_contact;d=c.incoming_attack
        report['contacts'].append(dict(case=label,t=now(),move=str(c.move_id),tier=str(c.tier),result=str(c.result),
            damage=c.actual_damage,position=report['frames'][-1]['position'],location=v(c.location),direction=v(c.direction),
            location_fallback=c.location_fallback,
            socket=str(d.trace_socket) if d else None,attacker_socket=v(p.mesh.get_socket_location(d.trace_socket)) if d else None,
            target_nearest_bone=min(['head','spine_03','pelvis'],key=lambda b:distance(v(c.location),v(q.mesh.get_socket_location(b)))),
            target_montage=q.mesh.get_anim_instance().get_current_active_montage().get_name() if q.mesh.get_anim_instance().get_current_active_montage() else None,
            gap=distance(v(p.get_actor_location()),v(q.get_actor_location()))))

def suite():
    global record,label,last_count
    cmd('t.MaxFPS 60');cmd('JJK.Feedback.HitStop 0');cmd('JJK.Feedback.Reaction 1')
    for mode in ['baseline','library']:
        fd.set_editor_property('reaction_library',old_library if mode=='baseline' else lib)
        for move,method,expected in [('A1','submit_light_attack',35),('HeavyPunch','submit_heavy_punch',90)]:
            for view in ['front','side']:
                reset();set_camera(view);yield from wait(.35)
                check(mode+'/'+move+'/'+view+' upright actors',abs(q.get_actor_rotation().pitch)<.01 and abs(q.get_actor_rotation().roll)<.01,str(q.get_actor_rotation()))
                label=mode+'-'+move+'-'+view;record=True;last_count=fp.contact_count;before=hp(q);count=fp.contact_count
                getattr(inp,method)();yield from until(lambda:fp.contact_count>count,1.6)
                check(label+' real hit',fp.contact_count==count+1 and abs(before-hp(q)-expected)<.01,before-hp(q))
                if fp.contact_count>count:
                    if mode=='library':check(label+' resolved montage',q.mesh.get_anim_instance().get_current_active_montage()==(lib.light if move=='A1' else lib.heavy))
                    yield from snap(label)
                yield from wait(1.2);record=False
                check(label+' recovers',q.can_act() and not q.mesh.get_anim_instance().montage_is_playing(lib.light) and not q.mesh.get_anim_instance().montage_is_playing(lib.heavy))
    for kind,method,segments,total in [('punch','submit_light_attack',4,185),('kick','submit_kick',3,135)]:
        reset();yield from wait(.3);record=True;label=kind+'-chain';last_count=fp.contact_count;before=hp(q);count=fp.contact_count
        for i in range(segments):
            getattr(inp,method)();yield from until(lambda:fp.contact_count>=count+i+1,1.7)
        yield from wait(1.4);record=False
        check(label+' all contacts and original damage',fp.contact_count==count+segments and abs(before-hp(q)-total)<.01,dict(contacts=fp.contact_count-count,damage=before-hp(q)))
        check(label+' clear reaction and recover',q.can_act() and not q.mesh.get_anim_instance().is_any_montage_playing())
    reset();yield from wait(.3);record=True;label='HeavyKick';last_count=fp.contact_count;before=hp(q);count=fp.contact_count
    inp.submit_heavy_kick();yield from wait(1.8);record=False
    check('HeavyKick damage and no knockdown',fp.contact_count==count+1 and before-hp(q)==da('HeavyKick').damage and not tag(q,'State.KnockedDown'),before-hp(q))
    for i in range(3):
        reset();yield from wait(.3);q.get_combat_input().notify_guard_pressed();count=fp.contact_count
        inp.submit_light_attack();yield from until(lambda:fp.contact_count>count)
        check('Guard '+str(i)+' real result and montage',fp.last_contact.result==unreal.CombatFeedbackResult.GUARD and q.mesh.get_anim_instance().get_current_active_montage()==lib.guard)
        yield from wait(.65)
        check('Guard '+str(i)+' keeps intent and clears impact',q.get_combat_input().is_guard_intent() and tag(q,'State.Guarding') and not tag(q,'State.GuardStun') and not q.mesh.get_anim_instance().montage_is_playing(lib.guard))
    # Repeatable visual sequences. Single attacks/chains still use real inputs.
    for view in ['side','front']:
        for move,method,duration in [('A1','submit_light_attack',.9),('HeavyPunch','submit_heavy_punch',1.5)]:
            reset();set_camera(view);yield from wait(.3);getattr(inp,method)()
            yield from motion('motion-'+move+'-'+view,duration)
    for kind,method,times,duration in [('PunchChain','submit_light_attack',[0,.2,.6,1.1],2.2),('KickChain','submit_kick',[0,.35,1.0],2.9)]:
        reset();set_camera('side');yield from wait(.3)
        start=now();folder=out/('motion-'+kind);folder.mkdir();frames=[];sent=0
        while now()-start<duration:
            if sent<len(times) and now()-start>=times[sent]:getattr(inp,method)();sent+=1
            image=folder/(str(len(frames)).zfill(4)+'.png');cmd('Shot -nosuffix filename='+image.as_posix());frames.append(dict(file=image.as_posix(),t=now()))
            yield from wait(.045)
        report['motion_captures']['motion-'+kind]=frames
    reset();set_camera('side');yield from wait(.3);q.get_combat_input().notify_guard_pressed()
    start_m=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardStart')
    loop_m=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardLoop')
    end_m=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_GuardEnd')
    q.play_anim_montage(start_m);yield from wait(.20);q.play_anim_montage(loop_m);yield from wait(.20)
    inp.submit_light_attack();yield from motion('motion-GuardImpact',.55)
    q.play_anim_montage(loop_m);yield from wait(.3);q.get_combat_input().notify_guard_released();q.play_anim_montage(end_m);yield from wait(.6)
    check('Isolated guard Start/Loop/Impact/End clears on release',not q.get_combat_input().is_guard_intent() and not q.mesh.get_anim_instance().is_any_montage_playing())
    for n in ['Light','Heavy','Guard']:
        reset();yield from wait(.2);m=getattr(lib,n.lower());q.play_anim_montage(m)
        yield from wait(.05);gm.reset_training();yield
        check('Reset stops '+n,not q.mesh.get_anim_instance().montage_is_playing(m))
    temp(da('HeavyPunch'),'grants_super_armor',True)
    reset();yield from wait(.3);q.get_combat_input().submit_heavy_punch();count=fp.contact_count;before=hp(q)
    inp.submit_light_attack();yield from until(lambda:fp.contact_count>count)
    check('Armor takes damage without replacing attack',before-hp(q)==35 and fp.last_contact.armored and q.mesh.get_anim_instance().get_current_active_montage()==da('HeavyPunch').montage)
    da('HeavyPunch').set_editor_property('grants_super_armor',original[-1][2])
    temp(fd,'initial_health',20.);reset();yield from wait(.3);count=fp.contact_count;inp.submit_light_attack();yield from until(lambda:fp.contact_count>count)
    check('Death wins over B reaction',q.is_dead() and fp.last_contact.lethal and q.mesh.get_anim_instance().get_current_active_montage() not in [lib.light,lib.heavy,lib.guard])
    fd.set_editor_property('initial_health',original[-1][2])
    # A's 120 FPS gate stays open. This is only a local, real-contact check that
    # B's entry pose is established before an enabled stop, at a 60 FPS cap.
    cmd('JJK.Feedback.HitStop 1');reset();yield from wait(.3)
    before=v(q.mesh.get_socket_location('head'));count=fp.contact_count;inp.submit_heavy_punch()
    yield from until(lambda:fp.contact_count>count)
    check('Heavy entry pose established before local stop',q.get_combat_feedback().is_stopped() and q.mesh.get_anim_instance().get_current_active_montage()==lib.heavy and distance(before,v(q.mesh.get_socket_location('head')))>5.,dict(before=before,after=v(q.mesh.get_socket_location('head'))))
    yield from wait(.8);cmd('JJK.Feedback.HitStop 0')
    # A still owns Fire and guard-intent consumers. No manual playback masquerades
    # as production feedback here; the real Fire is observed separately.
    reset(600);yield from wait(.3);q.get_combat_input().notify_stance_switch_pressed();yield from wait(.6)
    fq=q.get_combat_feedback();count=fq.fire_count;q.get_combat_input().notify_kick_pressed();yield from wait(2.8)
    q.get_combat_input().notify_kick_released();yield from until(lambda:fq.fire_count>count,1.)
    check('SuperBlast real Fire remains available',fq.fire_count==count+1)
    if fq.fire_count>count:
        pos=v(q.get_actor_location());recoil=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_SuperRecoil')
        q.play_anim_montage(recoil);yield from motion('motion-SuperRecoil-isolated-Fire',.5)
        check('Isolated Fire recoil ends without capsule motion',not q.mesh.get_anim_instance().montage_is_playing(recoil) and distance(pos,v(q.get_actor_location()))<.1)
    report['pending_A']=['saved ReactionLibrary binding','guard start/loop/end on actual intent','SuperRecoil from successful SuperBlast Fire; stop on lifecycle and interruption']
    report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed'

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.GameplayStatics.set_game_paused(w,False)
    for obj,k,old in reversed(original):obj.set_editor_property(k,old)
    fd.set_editor_property('reaction_library',old_library)
    for n,value in old_cvars.items():cmd(n+' '+str(value))
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    gm.reset_training();pc.set_combat_input_enabled(True);pc.set_view_target_with_blend(p,0.)
    report['seconds']=time.monotonic()-started;report['library_restored']=fd.reaction_library==old_library;write()
def tick(dt):
    try:sample();next(gen)
    except StopIteration:finish()
    except Exception:report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
