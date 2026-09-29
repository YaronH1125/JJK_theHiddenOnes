"""Real PIE regression for held melee, three kicks, recovery, and VFX removal."""
import json
import time
import traceback
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()
fd = p1.get_definition()
inp = p1.get_combat_input()
hit = p1.get_combat_hit()
anim = p1.mesh.get_anim_instance()
report = {'status': 'running', 'checks': []}
out = Path(unreal.Paths.project_saved_dir()) / 'IG20/acceptance.json'
perf = unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
subsystem = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary')).call_method(
    'GetLocalPlayerSubSystemFromPlayerController', (pc, unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
actions = {n: unreal.load_asset('/Game/Training/IA_' + n) for n in ['Attack', 'Kick']}

def write():
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')
def check(name, ok, detail=None):
    report['checks'].append(dict(name=name, passed=bool(ok), detail=detail))
    write()
def now():
    return unreal.GameplayStatics.get_time_seconds(world)
def wait(seconds):
    until = now() + seconds
    while now() < until:
        yield
def until(predicate, seconds=2):
    deadline = now() + seconds
    while not predicate() and now() < deadline:
        yield
def hp():
    return p2.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value')
def reset(close=True):
    gm.reset_training()
    pc.set_combat_input_enabled(True)
    for f, x, yaw in [(p1, 400 if close else -500, 0), (p2, 510, 180)]:
        f.character_movement.stop_movement_immediately()
        f.set_actor_location(unreal.Vector(x, 0, 100), False, True)
        f.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
def inject(name, value):
    subsystem.inject_input_vector_for_action(actions[name], unreal.Vector(value, 0, 0), [], [])
def tap(name):
    for _ in wait(.06):
        inject(name, 1)
        yield
    inject(name, 0)
    yield
    yield
def da(name):
    return unreal.load_asset('/Game/Training/DA_M3_' + name)

def suite():
    yield from wait(.6)
    check('three independent kick definitions', len(fd.get_editor_property('kick_segments')) == 3)
    for n in ['A1','A2','A3','A4','Kick','Kick2','Kick3','HeavyPunch','HeavyKick']:
        d = da(n)
        check(n + ' has no melee Niagara effect', d.get_editor_property('hit_effect') is None)
        check(n + ' hit window inside montage', 0 <= d.window_start_time < d.window_end_time < d.montage.get_play_length())
    check('Superpunch uses striking right hand', str(da('HeavyPunch').trace_socket) == 'hand_r')

    for fps in [30, 60, 120]:
        unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS ' + str(fps))
        reset()
        yield from wait(.3)
        start_hp = hp()
        # Enhanced Input holds are re-injected per frame, testing the real input binding.
        for _ in wait(4.5 if fps == 60 else 1.4):
            inject('Attack', 1)
            yield
        m = da('HeavyPunch').montage
        check(f'{fps} hold enters charge', inp.is_melee_charging())
        check(f'{fps} hold does not hit', hp() == start_hp and not hit.is_window_open())
        pos = anim.montage_get_position(m)
        check(f'{fps} holds windup pose', abs(pos - da('HeavyPunch').charge_hold_time) < .035, pos)
        inject('Attack', 0)
        yield from wait(1.15)
        check(f'{fps} release hits exactly once', abs(start_hp - hp() - da('HeavyPunch').damage) < .1,
              {'damage': start_hp - hp(), 'hits': hit.get_hit_count()})
        check(f'{fps} release clears charge and recovers', not inp.is_melee_charging() and p1.can_act())
        inp.notify_attack_released()
        yield from wait(.15)
        check(f'{fps} duplicate release cannot attack', p1.can_act() and not hit.has_active_attack())

    unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')
    reset()
    yield from wait(.25)
    start_hp = hp()
    yield from tap('Attack')
    yield from wait(.8)
    check('tap remains light A1 only', abs(start_hp - hp() - da('A1').damage) < .1 and p1.can_act(), start_hp - hp())

    reset()
    yield from wait(.25)
    start_hp = hp()
    inp.notify_attack_pressed()
    yield from wait(.23)
    inp.notify_attack_released()
    yield from wait(1.5)
    check('release during windup swings once', abs(start_hp - hp() - da('HeavyPunch').damage) < .1 and p1.can_act(), start_hp - hp())

    for method in ['hit', 'reset', 'menu', 'dodge', 'death']:
        reset(False)
        yield from wait(.2)
        inp.notify_attack_pressed()
        yield from wait(.9)
        check(method + ' starts from held charge', inp.is_melee_charging())
        if method == 'hit':
            p1.jjk_debug_force_hit_react()
        elif method == 'reset':
            gm.reset_training()
        elif method == 'menu':
            pc.set_combat_input_enabled(False)
        elif method == 'dodge':
            check('held charge accepts legal dodge cancel', p1.request_dodge(unreal.Vector(0, 1, 0)))
        else:
            p1.jjk_debug_kill()
        yield from wait(.15)
        inp.notify_attack_released()
        yield from wait(.9)
        check(method + ' cancels without stale heavy', not inp.is_melee_charging() and not hit.has_active_attack())

    reset()
    yield from wait(.25)
    start_hp = hp()
    inp.notify_kick_pressed()
    yield from wait(1.1)
    check('heavy kick holds without contact', inp.is_melee_charging() and hp() == start_hp)
    inp.notify_kick_released()
    yield from wait(1.2)
    check('heavy kick release once', abs(start_hp - hp() - da('HeavyKick').damage) < .1 and p1.can_act(), start_hp - hp())

    reset(False)
    yield from wait(.2)
    inp.notify_attack_pressed()
    yield from wait(1.2)
    inp.notify_attack_released()
    yield from wait(.4)
    check('released heavy recovery permits configured dodge', p1.request_dodge(unreal.Vector(0, 1, 0)))
    yield from wait(.8)

    reset(False)
    yield from wait(.2)
    yield from tap('Kick')
    yield from tap('Kick')
    yield from until(lambda: hit.get_segment_id() == 7)
    check('rapid double tap buffers K2', hit.get_segment_id() == 7)
    yield from wait(1.2)
    check('rapid double tap never auto plays K3', hit.get_segment_id() == 7 and p1.can_act())

    reset()
    yield from wait(.25)
    start_hp = hp()
    yield from tap('Kick')
    yield from wait(.43)
    yield from tap('Kick')
    yield from until(lambda: hit.get_segment_id() == 7)
    check('second Q advances to K2', hit.get_segment_id() == 7, hit.get_segment_id())
    yield from wait(.40)
    yield from tap('Kick')
    yield from until(lambda: hit.get_segment_id() == 8)
    check('third Q advances to K3', hit.get_segment_id() == 8, hit.get_segment_id())
    yield from wait(1.3)
    check('kick chain hits each once and ends', abs(start_hp-hp()-sum(da(n).damage for n in ['Kick','Kick2','Kick3'])) < .1 and p1.can_act(), start_hp-hp())

    reset(False)
    yield from wait(.2)
    yield from tap('Kick')
    yield from wait(.35)
    yield from tap('Attack')
    yield from wait(1)
    check('LMB cannot accidentally advance kick chain', hit.get_segment_id() == 4 and p1.can_act())

    reset()
    yield from wait(.25)
    start_hp = hp()
    inp.submit_light_attack()
    for expected, delay in [(1,.22),(2,.25),(6,.35)]:
        yield from wait(delay)
        inp.submit_light_attack()
        yield from until(lambda: hit.get_segment_id() == expected)
        check('punch chain advances to ' + str(expected), hit.get_segment_id() == expected)
    yield from wait(1.35)
    check('four punches hit once each', abs(start_hp-hp()-sum(da(n).damage for n in ['A1','A2','A3','A4'])) < .1, start_hp-hp())
    check('punch chain ends unlocked', p1.can_act())

    for name, method in [('A1','submit_light_attack'), ('Kick','submit_kick')]:
        reset(False)
        yield from wait(.2)
        start = now()
        getattr(inp, method)()
        yield from until(lambda: p1.can_act(), 2)
        check(name + ' no excess recovery lock', now()-start < da(name).montage.get_play_length()+.15, now()-start)

gen = suite()
started = time.monotonic()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 0')
    gm.reset_training()
    pc.set_combat_input_enabled(True)
    report['duration_seconds'] = time.monotonic()-started
    write()
def tick(dt):
    try:
        if time.monotonic()-started > 160:
            raise TimeoutError('IG20 acceptance timed out')
        next(gen)
    except StopIteration:
        report['status'] = 'passed' if all(c['passed'] for c in report['checks']) else 'failed'
        finish()
    except Exception:
        report['status'] = 'failed'
        report['error'] = traceback.format_exc()
        finish()
write()
handle = unreal.register_slate_post_tick_callback(tick)
print('IG20 PIE acceptance started')
