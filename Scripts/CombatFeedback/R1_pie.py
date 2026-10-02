"""R1 real-input regression: heavy armor confirmation, clock, camera and VFX tails.
Launched exclusively by R1_run.py; temporary fixtures are restored, never saved.
No synthetic contacts; normal damage and costs remain in use.
"""
import json
import math
import statistics
import time
import traceback
from pathlib import Path
import unreal

run_info = json.loads((Path(unreal.Paths.project_saved_dir())/'FeedbackRevisionR1/latest-run.json').read_text(encoding='utf-8'))
out = Path(run_info['path'])
capture_enabled = run_info.get('capture', False)
visual_only = run_info.get('visual_only', False)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc = unreal.GameplayStatics.get_player_controller(w, 0)
gm = pc.get_training_game_mode()
p, q = gm.get_player_fighter(), gm.get_opponent_fighter()
fp, fq = p.get_combat_feedback(), q.get_combat_feedback()
pv = p.get_component_by_class(unreal.CombatRangedVisualConsumer)
cam = pc.combat_camera_feedback
report = {'status': 'running', 'candidate': 'FR1-20261001', 'checks': [], 'fps': [], 'captures': [],
          'human_listening': False, 'human_input': False, 'visual_only': visual_only,
          'clock_render_fixture': 'lower rendering cost for actual 120 tick test, not a performance claim'}
old = {}
frames = []

def write(): (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
def check(name, passed, detail=None):
    report['checks'].append({'name': name, 'passed': bool(passed), 'detail': detail}); write()
def now(): return unreal.GameplayStatics.get_time_seconds(w)
def wait(t):
    end = now()+t
    while now() < end: yield
def until(fn, t=3):
    end = now()+t
    while not fn() and now() < end: yield
    assert fn(), 'Timed out waiting for real state'
def cmd(name, value):
    if name not in old: old[name] = unreal.SystemLibrary.get_console_variable_float_value(name)
    unreal.SystemLibrary.execute_console_command(w, name+' '+str(value), pc)
def hp(f): return f.get_fighter_attribute_set().health.current_value
def reset(gap=120):
    pc.handle_app_activation_changed(True); pc.set_training_panel_open(False)
    gm.set_opponent_mode(unreal.OpponentMode.STATIC); gm.jjk_clear_blockers(); gm.reset_training()
    pc.set_combat_input_enabled(True)
    pc.set_control_rotation(unreal.Rotator(pitch=-8, yaw=40, roll=0))
    for f, x, yaw in [(p,0,0), (q,gap,180)]:
        f.character_movement.set_component_tick_enabled(True)
        f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x,0,100),False,True)
        f.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
def shot(name):
    if not capture_enabled: return
    path = out/(name+'.png')
    unreal.SystemLibrary.execute_console_command(w,'Shot showui -nosuffix filename='+path.as_posix(),pc)
    report['captures'].append(str(path)); write()
def projectiles(): return unreal.GameplayStatics.get_all_actors_of_class(w,unreal.BlastProjectile)
def head(f):
    v = f.mesh.get_socket_location('head'); return [v.x,v.y,v.z]
def dist(a,b): return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))

def melee_sample(label, method, damage, stop, capture=False):
    reset(); yield from wait(.4)
    before = hp(q); count = fp.contact_count; pos = head(q)
    getattr(p.get_combat_input(),method)()
    yield from until(lambda: fp.contact_count>count)
    check(label+' one real contact and original damage',fp.contact_count==count+1 and hp(q)==before-damage)
    check(label+' both participants freeze on own confirmation',fp.is_stopped() and fq.is_stopped())
    check(label+' correct tier',fp.last_contact.tier == (unreal.CombatFeedbackTier.HEAVY if damage==90 else unreal.CombatFeedbackTier.LIGHT))
    pose_distance = dist(pos,head(q))
    if damage==90:
        check(label+' heavy holds visible impact pose',pose_distance>3.,pose_distance)
        check(label+' stronger camera bounded at 0.6 deg',.59<=cam.peak_roll<=.601,cam.peak_roll)
    if capture: shot(label+'-held-pose')
    montage = p.mesh.get_anim_instance().get_current_active_montage()
    position = p.mesh.get_anim_instance().montage_get_position(montage)
    action = p.get_action_time(); held=True; samples=[]
    while fp.is_stopped():
        held &= abs(p.get_action_time()-action)<.0001 and abs(p.mesh.get_anim_instance().montage_get_position(montage)-position)<.0001
        samples.append(unreal.GameplayStatics.get_world_delta_seconds(w)); yield
    # Include the resume frame itself: screenshot/readback or PSO stalls can
    # occur on that frame, and must remain visible in the observed tick bound.
    actual = fp.get_last_stop_duration(); tick = max(samples + [unreal.GameplayStatics.get_world_delta_seconds(w)])
    check(label+' consistent local clock and montage freeze',held)
    check(label+' stop quantization within one actual tick',stop-.001<=actual<=stop+tick+.003,
          {'configured':stop,'actual':actual,'max_tick':tick,'head_displacement':pose_distance})
    yield from wait(1.1)
    check(label+' restores action and input',p.can_act() and q.can_act() and not fp.is_stopped() and not fq.is_stopped())
    return actual

def blast_sample(label, wall=False):
    reset(600); yield from wait(.7)
    # The opponent uses its actual target aim; the player's free camera ray
    # intentionally is not an automatic aim-at-dummy shortcut.
    q.get_combat_input().notify_stance_switch_pressed(); yield from wait(.6)
    source = fq; visual=q.get_component_by_class(unreal.CombatRangedVisualConsumer)
    before = hp(p); contacts=source.contact_count; cues=visual.spawned_cue_count
    natural=visual.natural_retired_cue_count; forced=visual.forced_retired_cue_count
    q.get_combat_input().notify_kick_pressed()
    yield from until(lambda:source.last_action.paid_q>.999,5)
    if wall: gm.jjk_spawn_blocker(300.,0.,.3,6.,5.)
    q.get_combat_input().notify_kick_released()
    yield from until(lambda:source.contact_count>contacts)
    expected = unreal.CombatFeedbackResult.WORLD_IMPACT if wall else unreal.CombatFeedbackResult.HIT
    check(label+' one settled result and one burst',source.contact_count==contacts+1 and source.last_contact.result==expected and visual.spawned_cue_count==cues+1)
    check(label+' unchanged damage',hp(p)==before-(0 if wall else 220),before-hp(p))
    check(label+' beam retained visually after settlement',len(projectiles())==1,len(projectiles()))
    check(label+' retained projectile cannot collide',all(not a.get_actor_enable_collision() for a in projectiles()))
    shot(label+'-contact'); yield from wait(.22); shot(label+'-hold')
    check(label+' has bounded emission phase',visual.active_cue_count==1 and visual.draining_cue_count==0)
    yield from wait(.36); shot(label+'-natural-tail')
    # Burst-only emitters may finish their authored fade before emission cutoff;
    # they do not have to remain artificially alive to qualify as a natural end.
    yield from wait(.8); shot(label+'-cleared')
    check(label+' particles finish naturally without hard cutoff',visual.natural_retired_cue_count==natural+1 and visual.forced_retired_cue_count==forced,
          {'natural_retired':visual.natural_retired_cue_count-natural,'forced_retired':visual.forced_retired_cue_count-forced})
    check(label+' tails reclaimed and no repeated contact',not projectiles() and visual.active_cue_count==0 and source.contact_count==contacts+1)
    gm.jjk_clear_blockers()

def suite():
    cmd('t.MaxFPS',60); cmd('JJK.Feedback.HitStop',1)
    for key in ['Reaction','Audio','RangedFX','Camera','HUD']: cmd('JJK.Feedback.'+key,1)
    cmd('JJK.Feedback.CameraStrength',2)
    yield from melee_sample('light','submit_light_attack',35,.022,True)
    yield from melee_sample('heavy','submit_heavy_punch',90,.090,True)
    if visual_only:
        yield from blast_sample('super-hit')
        yield from blast_sample('super-wall',True)
        return
    # Shared input buffering remains live during stop; no injected contact.
    reset(); yield from wait(.4); before=hp(q); contacts=fp.contact_count
    for i in range(4):
        p.get_combat_input().submit_light_attack()
        yield from until(lambda:fp.contact_count>=contacts+i+1)
    yield from wait(1.2)
    check('four punches remain 185 with no duplicate input',hp(q)==before-185 and fp.contact_count==contacts+4)
    reset(); yield from wait(.4); before=hp(q); contacts=fp.contact_count
    for i in range(3):
        p.get_combat_input().submit_kick()
        yield from until(lambda:fp.contact_count>=contacts+i+1)
    yield from wait(1.2)
    check('three kicks remain 135',hp(q)==before-135 and fp.contact_count==contacts+3)
    yield from blast_sample('super-hit')
    yield from blast_sample('super-wall',True)
    # Reset during emission/tail always remains immediate, with no stale cues.
    for i in range(5):
        reset(600); yield from wait(.4)
        p.get_combat_input().notify_stance_switch_pressed(); yield from wait(.6)
        p.get_combat_input().notify_attack_pressed(); yield from wait(.4)
        contacts=fp.contact_count; p.get_combat_input().notify_attack_released()
        yield from until(lambda:fp.contact_count>contacts)
        yield from wait(.48 if i%2 else .12)
        gm.reset_training(); yield
        check('reset '+str(i+1)+' clears beam/cue tails immediately',not projectiles() and pv.active_cue_count==0)
    # Explicit clock stress only: real world deltas, no fixed-step fiction.
    for key,value in [('r.ScreenPercentage',25),('r.DynamicGlobalIlluminationMethod',0),('r.ReflectionMethod',0),
                      ('r.ShadowQuality',0),('r.AntiAliasingMethod',0),('r.VolumetricFog',0),('r.VSync',0),
                      ('ShowFlag.StaticMeshes',0),('ShowFlag.PostProcessing',0),('ShowFlag.Lighting',0),
                      ('ShowFlag.Translucency',0),('ShowFlag.Atmosphere',0)]: cmd(key,value)
    for fps in [30,60,120]:
        cmd('t.MaxFPS',fps); reset(); yield from wait(.6)
        frames.clear(); yield from wait(1.5)
        measured=1/statistics.mean(frames); detail={'cap':fps,'actual_fps':measured,'max_tick':max(frames)}
        report['fps'].append(detail)
        check('actual '+str(fps)+' FPS test environment reached',measured>=fps*.99,detail)
        yield from melee_sample(str(fps)+'-light','submit_light_attack',35,.022)
        yield from melee_sample(str(fps)+'-heavy','submit_heavy_punch',90,.090)

started=time.monotonic(); gen=suite()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    for key,value in old.items(): unreal.SystemLibrary.execute_console_command(w,key+' '+str(value),pc)
    gm.jjk_clear_blockers();gm.reset_training();pc.set_combat_input_enabled(True)
    report['seconds']=time.monotonic()-started;write()
def tick(dt):
    frames.append(unreal.GameplayStatics.get_world_delta_seconds(w))
    try:
        if time.monotonic()-started>240: raise TimeoutError('R1 regression')
        next(gen)
    except StopIteration: report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed';finish()
    except Exception: report['status']='failed';report['error']=traceback.format_exc();finish()
write();handle=unreal.register_slate_post_tick_callback(tick)
