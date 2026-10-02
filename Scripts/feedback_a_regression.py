"""Feedback A real-contact and local-clock regression in PIE.

Run: python Scripts/run_feedback_a.py regression
No assets are saved. Temporary data and CVars are restored even on failure.
Synthetic clock stress is explicitly named and kept separate from real contacts.
"""
import json
import time
import traceback
import statistics
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(w,0)
gm=pc.get_training_game_mode()
p,q=gm.get_player_fighter(),gm.get_opponent_fighter()
fp,fq=p.get_combat_feedback(),q.get_combat_feedback()
inp=p.get_combat_input(); hit=p.get_combat_hit(); fd=p.get_definition()
out=Path(unreal.Paths.project_saved_dir())/'FeedbackA/regression.json'
r={'status':'running','checks':[],'fps':[],'engine':unreal.SystemLibrary.get_engine_version()}
original=[]; frames=[]
perf=unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
old_cvars={n:unreal.SystemLibrary.get_console_variable_int_value(n) for n in ['t.MaxFPS','JJK.Feedback.HitStop','JJK.Feedback.Reaction','JJK.Feedback.Audio','JJK.Feedback.Camera','JJK.Feedback.RangedFX','JJK.Feedback.HUD','r.ScreenPercentage','r.DynamicGlobalIlluminationMethod','r.ReflectionMethod','r.ShadowQuality','r.AntiAliasingMethod','r.VolumetricFog','r.VSync','ShowFlag.StaticMeshes','ShowFlag.PostProcessing','ShowFlag.Lighting','ShowFlag.Translucency','ShowFlag.Atmosphere']}
def write(): out.write_text(json.dumps(r,indent=2),encoding='utf-8')
def check(n,b,d=None):
    r['checks'].append({'name':n,'passed':bool(b),'detail':d});write()
def now(): return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end=now()+t
    while now()<end: yield
def until(pred,t=3):
    end=now()+t
    while not pred() and now()<end: yield
def hp(f): return f.get_fighter_attribute_set().health.current_value
def tag(f,n):
    t=unreal.GameplayTag();t.import_text('(TagName="'+n+'")');return f.has_combat_tag(t)
def cmd(s): unreal.SystemLibrary.execute_console_command(w,s)
def temp(o,k,v):
    if not any(a==o and b==k for a,b,c in original):original.append((o,k,o.get_editor_property(k)))
    o.set_editor_property(k,v)
def reset(gap=150):
    gm.reset_training();pc.set_combat_input_enabled(True)
    for f,x,yaw in [(p,0,0),(q,gap,180)]:
        f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def da(n):return unreal.load_asset('/Game/Training/DA_M3_'+n)
def shots():return unreal.GameplayStatics.get_all_actors_of_class(w,unreal.BlastProjectile)
def ranged(f):
    f.get_combat_input().notify_stance_switch_pressed()
    yield from wait(.6)
def suite():
    cmd('JJK.Feedback.HitStop 1')
    r['clock_render_settings']='ScreenPercentage 25; GI/reflections/shadows/AA/volumetric fog off; static meshes/postprocessing/lighting/translucency/atmosphere hidden for clock test; original CVars restored'
    for n,v in [('r.ScreenPercentage',25),('r.DynamicGlobalIlluminationMethod',0),('r.ReflectionMethod',0),('r.ShadowQuality',0),('r.AntiAliasingMethod',0),('r.VolumetricFog',0),('r.VSync',0),('ShowFlag.StaticMeshes',0),('ShowFlag.PostProcessing',0),('ShowFlag.Lighting',0),('ShowFlag.Translucency',0),('ShowFlag.Atmosphere',0)]:cmd(n+' '+str(v))
    for fps in [30,60,120]:
        cmd('t.MaxFPS '+str(fps));reset();yield from wait(.6)
        frames.clear();yield from wait(1)
        sample=list(frames);measured=1/statistics.mean(sample)
        r['fps'].append({'cap':fps,'actual_mean':measured,'median':1/statistics.median(sample),'max_tick':max(sample),'samples':len(sample)})
        check(f'T17 achieved {fps} fps',measured>=fps*.99,r['fps'][-1])
        before=hp(q);count=fp.contact_count;inp.submit_light_attack()
        yield from until(lambda:fp.contact_count>count)
        c=fp.last_contact
        check(f'T01 {fps} A1 one real hit',fp.contact_count==count+1 and c.result==unreal.CombatFeedbackResult.HIT and hp(q)==before-35,str(c.result))
        check(f'T05 {fps} stun applied before freeze',tag(q,'State.HitStun') and fp.is_stopped() and fq.is_stopped())
        start_action=p.get_action_time();montage=da('A1').montage
        pos=p.mesh.get_anim_instance().montage_get_position(montage)
        frozen_ok=True;pose_ok=True
        inp.submit_light_attack() # actual legal cache during stop
        while fp.is_stopped():
            frozen_ok &= abs(p.get_action_time()-start_action)<.0001
            pose_ok &= abs(p.mesh.get_anim_instance().montage_get_position(montage)-pos)<.0001
            yield
        duration=fp.get_last_stop_duration()
        check(f'T03 {fps} local clock and pose held',frozen_ok and pose_ok)
        light_stop=fp.get_profile().light_stop
        check(f'T17 {fps} stop within one observed tick',light_stop-.001<=duration<=light_stop+max(sample)+.003,{'actual':duration,'configured':light_stop,'max_tick':max(sample)})
        yield from wait(1.3)
        check(f'T03 {fps} cache consumed once',fp.contact_count==count+2 and hp(q)==before-75,{'contacts':fp.contact_count-count,'damage':before-hp(q)})
        check(f'T05 {fps} tag and action lock recover',not tag(q,'State.HitStun') and q.can_act())
        reset();yield from wait(.2)
        q.get_combat_input().notify_guard_pressed();count=fp.contact_count;before=hp(q)
        inp.submit_light_attack();yield from until(lambda:fp.contact_count>count)
        check(f'T01 {fps} guard classified',fp.last_contact.result==unreal.CombatFeedbackResult.GUARD and tag(q,'State.GuardStun'))
        yield from wait(.7)
        check(f'T05 {fps} guard stun recovers with intent',not tag(q,'State.GuardStun') and q.get_combat_input().is_guard_intent())
        reset(900);yield from wait(.2)
        count=fp.contact_count;inp.submit_light_attack();yield from wait(.8)
        check(f'T01 {fps} whiff has no contact',fp.contact_count==count and not fp.is_stopped())

        reset();yield from wait(.3);before=hp(q);count=fp.contact_count
        inp.submit_heavy_punch();yield from until(lambda:fp.contact_count>count)
        check(f'T02 {fps} heavy real damage and tier',hp(q)==before-90 and fp.last_contact.tier==unreal.CombatFeedbackTier.HEAVY
              and fp.last_action.tier==fp.last_contact.tier)
        stop_ticks=[]
        while fp.is_stopped():
            stop_ticks.append(unreal.GameplayStatics.get_world_delta_seconds(w));yield
        step=max(stop_ticks or [unreal.GameplayStatics.get_world_delta_seconds(w)])
        heavy_stop=fp.get_profile().heavy_stop
        check(f'T17 {fps} heavy stop within one observed tick',heavy_stop-.001<=fp.get_last_stop_duration()<=heavy_stop+step+.001,
              {'actual':fp.get_last_stop_duration(),'configured':heavy_stop,'max_tick':step})

        # Short full-charge/release and stale-release paths at every FPS;
        # the 60-second hold below remains a separate long-duration test.
        reset(600);yield from wait(.7);yield from ranged(q)
        fires=fq.fire_count;full=fq.full_count;count=fq.contact_count
        q.get_combat_input().notify_attack_pressed();yield from wait(3.3)
        check(f'T09 {fps} actual PaidQ full exactly once',fq.last_action.paid_q>.999 and fq.full_count==full+1)
        q.get_combat_input().notify_attack_released();yield from until(lambda:fq.contact_count>count)
        check(f'T09 {fps} real Fire and full-strength contact once',fq.fire_count==fires+1 and fq.contact_count==count+1 and fq.last_contact.paid_q>.999)
        reset(600);yield from wait(.7);yield from ranged(q)
        q.get_combat_input().notify_attack_pressed();yield from wait(.3);fires=fq.fire_count
        gm.reset_training();q.get_combat_input().notify_attack_released();yield from wait(.6)
        check(f'T10 {fps} reset invalidates charge and stale release',fq.fire_count==fires and not tag(q,'State.BlastCharging'))

    cmd('t.MaxFPS 60')
    # All channels disabled, same settlement and resource path.
    for n in old_cvars:
        if n.startswith('JJK.'):cmd(n+' 0')
    reset();yield from wait(.3)
    before=hp(q);inp.submit_heavy_punch();yield from wait(1.3)
    check('T16 all channels off keeps real damage',hp(q)==before-90 and p.can_act() and q.can_act())
    for n in old_cvars:
        if n.startswith('JJK.'):cmd(n+' 1')
    # Menu/death/reset destroy the held clock and old input session.
    for method in ['menu','death']:
        reset();yield from wait(.2);count=fp.contact_count;inp.submit_light_attack()
        yield from until(lambda:fp.contact_count>count)
        if method=='menu':pc.set_training_panel_open(True)
        else:p.jjk_debug_kill();yield
        check('T10 '+method+' clears stop',not fp.is_stopped())
        inp.notify_attack_released()
        if method=='menu':pc.set_training_panel_open(False)
    for i in range(20):
        reset();yield from wait(.12);count=fp.contact_count;inp.submit_light_attack()
        yield from until(lambda:fp.contact_count>count)
        old_round=gm.get_feedback_round_id();old_gen=fp.get_generation()
        gm.reset_training();pc.set_combat_input_enabled(True)
        inp.notify_attack_released()
        check(f'T15 reset {i+1} invalidates old round and restores mesh/movement',not fp.is_stopped() and not fq.is_stopped()
              and not fp.is_current(old_round,old_gen) and p.character_movement.is_component_tick_enabled()
              and not p.mesh.get_editor_property('pause_anims') and not p.is_attacking())

    # AI-side aim uses real target direction, avoiding camera setup in the test.
    reset(600);yield from wait(.7);yield from ranged(q)
    count=fq.contact_count;fires=fq.fire_count;before=hp(p)
    q.get_combat_input().notify_kick_pressed();yield from wait(3.3)
    full=fq.full_count;paid=fq.last_action.paid_q
    yield from wait(60)
    check('T09 PaidQ full once during 60s hold',paid>.999 and fq.full_count==full and fq.last_action.paid_q>.999,{'full_count':full,'paid_q':paid})
    q.get_combat_input().notify_kick_released();yield from until(lambda:fq.contact_count>count)
    check('T07 super real spawn/hit and victim-only stop',fq.fire_count==fires+1 and fq.last_contact.result==unreal.CombatFeedbackResult.HIT
          and hp(p)==before-220 and not fq.is_stopped() and fp.is_stopped(),{'damage':before-hp(p),'result':str(fq.last_contact.result)})
    check('T08 actual surface contact',abs(fq.last_contact.location.x-p.get_actor_location().x)<100 and fq.last_contact.location.length()>1,str(fq.last_contact.location))
    yield from wait(1)
    # Cancelled charge and stale release cannot fire into new generation.
    for method in ['reset','menu','hit']:
        reset(600);yield from wait(.7);yield from ranged(q)
        q.get_combat_input().notify_attack_pressed();yield from wait(.3)
        count=fq.fire_count
        if method=='reset':gm.reset_training()
        elif method=='menu':gm.set_training_menu_open(True)
        else:q.jjk_debug_force_hit_react();yield
        q.get_combat_input().notify_attack_released();yield from wait(.6)
        check('T10 ranged '+method+' stale release',fq.fire_count==count and not tag(q,'State.BlastCharging'))
        if method=='menu':gm.set_training_menu_open(False)

gen=suite();started=time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for o,k,v in reversed(original):o.set_editor_property(k,v)
    for n,v in old_cvars.items():cmd(n+' '+str(v))
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    gm.set_training_menu_open(False);gm.reset_training();pc.set_combat_input_enabled(True)
    r['seconds']=time.monotonic()-started;write()
def tick(dt):
    try:
        frames.append(unreal.GameplayStatics.get_world_delta_seconds(w))
        if time.monotonic()-started>440:raise TimeoutError()
        next(gen)
    except StopIteration:r['status']='passed' if all(c['passed'] for c in r['checks']) else 'failed';finish()
    except Exception:r['status']='failed';r['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
