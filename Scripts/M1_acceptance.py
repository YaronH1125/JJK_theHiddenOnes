"""M1 PIE regression; run via ue_python.py, poll Saved/M1_acceptance.json.
Exercises real Enhanced Input action bindings, movement/collision, camera and GAS.
All temporary runtime changes are reset. Does not save assets during PIE.
"""
import json
import math
from pathlib import Path
import time
import traceback
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M1_acceptance.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()
targeting = p1.get_targeting()
input_sub = unreal.get_default_object(unreal.load_class(None, "/Script/Engine.SubsystemBlueprintLibrary")).call_method("GetLocalPlayerSubSystemFromPlayerController", (pc, unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
performance = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
state = {'status': 'running', 'engine': unreal.SystemLibrary.get_engine_version(), 'checks': []}
original_definition = p1.get_editor_property('definition')


def write():
    out.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, condition, details=None):
    state['checks'].append({'name': name, 'passed': bool(condition), 'details': details})
    write()
    assert condition, f'{name}: {details}'
    unreal.log('[M1Acceptance] PASS ' + name)


def pos(actor):
    p = actor.get_actor_location()
    return [round(p.x, 3), round(p.y, 3), round(p.z, 3)]


def attributes(actor):
    values = unreal.get_default_object(unreal.AbilitySystemInspectorToolset).call_method('GetAttributeValues', (actor,))
    return {v.attribute_name: v.current_value for v in values}


def wait(seconds, action=None):
    end = unreal.GameplayStatics.get_time_seconds(world) + seconds
    while unreal.GameplayStatics.get_time_seconds(world) < end:
        if action:
            action()
        yield


def inject(action, x=1, y=0):
    input_sub.inject_input_vector_for_action(action, unreal.Vector(x, y, 0), [], [])


def move_to(actor, x, y, z=100):
    actor.character_movement.stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)


def suite():
    global p2
    fighters = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)
    check('T01_two_fighters', len(fighters) == 2, [f.get_name() for f in fighters])
    check('T01_spawn_transform', pos(p1)[:2] == [-500, 0] and pos(p2)[:2] == [500, 0]
          and abs(p1.get_actor_rotation().yaw) < 1 and abs(abs(p2.get_actor_rotation().yaw) - 180) < 1,
          [pos(p1), pos(p2)])
    check('T01_possession_identity', p1.get_controller() == pc and p2.get_controller() is None
          and gm.get_opponent_of(p1) == p2 and gm.get_opponent_of(p2) == p1)
    check('T07_independent_GAS', p1.get_fighter_ability_system_component() != p2.get_fighter_ability_system_component()
          and p1.get_fighter_attribute_set() != p2.get_fighter_attribute_set()
          and p1.definition == p2.definition, [attributes(p1), attributes(p2)])
    check('T07_initial_stats', attributes(p1) == attributes(p2) == {'Health': 1000, 'MaxHealth': 1000,
          'ActionResource': 100, 'MaxActionResource': 100, 'Energy': 0, 'MaxEnergy': 100})
    for _ in range(3):
        gm.ensure_fighters_spawned()
        pc.possess(p1)
        p1.initialize_from_definition()
        p2.initialize_from_definition()
    check('T07_repossess_idempotent', p1.get_stats_init_count() == p2.get_stats_init_count() == 1
          and len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)) == 2)
    transient_definition = unreal.FighterDefinition()
    transient_definition.set_editor_property('initial_health', 500)
    p1.set_editor_property('definition', transient_definition)
    p1.reset_to_initial_state()
    p1.initialize_from_definition()
    p2.initialize_from_definition()
    check('T07_reset_isolation', attributes(p1)['Health'] == 500 and attributes(p2)['Health'] == 1000
          and original_definition.get_editor_property('initial_health') == 1000,
          [attributes(p1), attributes(p2)])
    p1.set_editor_property('definition', original_definition)
    p1.reset_to_initial_state()
    yield from wait(0.2)
    check('T08_static_components', p2.is_actor_tick_enabled() and p2.mesh.is_component_tick_enabled()
          and p2.mesh.get_anim_instance() is not None and attributes(p2)['Health'] == 1000
          and p2.capsule_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
          and p2.character_movement.is_moving_on_ground(), pos(p2))
    imc = p1.get_editor_property('default_mapping_context')
    check('T03_mapping_active', input_sub.has_mapping_context(imc) is not None)
    mapping_data = imc.get_editor_property('default_key_mappings')
    keys = {str(m.key.get_editor_property('key_name')): m.action.get_name() for m in mapping_data.mappings}
    check('T04_key_mapping', keys.get('MiddleMouseButton') == 'IA_LockTarget' and keys.get('C') == 'IA_RecenterCamera'
          and all(k in keys for k in ['W','A','S','D','Mouse2D']), keys)
    pc.set_control_rotation(unreal.Rotator(pitch=-10, yaw=90, roll=0))
    before = pos(p1)
    yield from wait(0.8, lambda: inject(p1.get_editor_property('move_action'), 0, 1))
    yield from wait(0.2)
    after = pos(p1)
    check('T03_camera_relative_movement', after[1] - before[1] > 100 and abs(after[0] - before[0]) < 10, [before, after])
    check('T02_opponent_uncontrolled', pos(p2)[:2] == [500, 0], pos(p2))
    lock = pc.get_editor_property('lock_target_action')
    recenter = pc.get_editor_property('recenter_camera_action')
    inject(lock)
    yield from wait(0.15)
    check('T04_lock_input_binding', targeting.get_current_target() == p2)
    before_yaw = pc.get_control_rotation().yaw
    yield from wait(0.25, lambda: inject(p1.get_editor_property('mouse_look_action'), 4, 0))
    after_yaw = pc.get_control_rotation().yaw
    check('T04_free_look_input_binding', abs(after_yaw - before_yaw) > 10 and targeting.get_current_target() == p2,
          [before_yaw, after_yaw])
    pc.set_control_rotation(unreal.Rotator(pitch=-10, yaw=170, roll=0))
    yield from wait(0.25)
    check('T04_no_camera_forcing', abs(pc.get_control_rotation().yaw - 170) < 1)
    inject(recenter)
    yield from wait(0.15)
    diff = p2.get_actor_location() - p1.get_actor_location()
    expected = math.degrees(math.atan2(diff.y, diff.x))
    check('T04_recenter_input_binding', abs(pc.get_control_rotation().yaw - expected) < 1,
          [pc.get_control_rotation().yaw, expected])
    inject(lock)
    yield from wait(0.15)
    check('T04_unlock_input_binding', targeting.get_current_target() is None)
    pc.set_control_rotation(unreal.Rotator(pitch=89, yaw=0, roll=0))
    yield from wait(0.15)
    check('T03_pitch_limit', abs(pc.get_control_rotation().pitch) <= 65.1, pc.get_control_rotation().pitch)

    # Radial capsule sweeps cover every wall and every seam at 5 degree spacing.
    radii = []
    for degree in range(0, 360, 5):
        a = math.radians(degree)
        move_to(p1, 850 * math.cos(a), 850 * math.sin(a))
        p1.set_actor_location(unreal.Vector(1500 * math.cos(a), 1500 * math.sin(a), 100), True, False)
        p = p1.get_actor_location()
        radii.append(round(math.hypot(p.x, p.y), 2))
    check('T02_boundary_72_sweeps', all(1000 < r < 1150 for r in radii), {'min': min(radii), 'max': max(radii)})
    # Drive one full circle using actual CharacterMovement input.
    move_to(p1, 950, 0)
    pc.set_control_rotation(unreal.Rotator(pitch=-10, yaw=0, roll=0))
    for degree in range(15, 361, 15):
        x, y = 950 * math.cos(math.radians(degree)), 950 * math.sin(math.radians(degree))
        deadline = unreal.GameplayStatics.get_time_seconds(world) + 2
        while True:
            p = p1.get_actor_location()
            dx, dy = x - p.x, y - p.y
            if math.hypot(dx, dy) < 45:
                break
            assert unreal.GameplayStatics.get_time_seconds(world) < deadline, f'Circle stuck at {degree}: {pos(p1)}'
            p1.do_move(dy / max(math.hypot(dx, dy), 1), dx / max(math.hypot(dx, dy), 1))
            yield
    check('T02_walk_full_circle', p1.character_movement.is_moving_on_ground() and pos(p1)[2] > 90, pos(p1))
    move_to(p1, 350, 0)
    pc.set_control_rotation(unreal.Rotator(pitch=-10, yaw=0, roll=0))
    yield from wait(0.5, lambda: p1.do_move(0, 1))
    check('T02_fighter_collision', (p1.get_actor_location() - p2.get_actor_location()).length() >= 82, [pos(p1), pos(p2)])

    boom = p1.get_component_by_class(unreal.SpringArmComponent)
    move_to(p1, -1065, 0)
    yield from wait(0.3)
    check('T05_wall_camera_retraction', boom.is_collision_fix_applied(), str(boom.get_socket_location('SpringEndpoint')))
    check('T05_camera_stays_inside', boom.get_socket_location('SpringEndpoint').x > -1125)
    move_to(p1, 0, -300)
    yield from wait(0.3)
    check('T05_camera_recovers', not boom.is_collision_fix_applied())
    targeting.lock_target(p2)
    yaw = pc.get_control_rotation().yaw
    move_to(p2, 5000, 0)
    yield from wait(0.15)
    check('T06_out_of_range_clears', not targeting.is_target_valid() and targeting.get_current_target() is None
          and not targeting.is_component_tick_enabled())
    p2.reset_to_initial_state()
    yield from wait(0.15)
    check('T06_relock', targeting.lock_best_target())
    p2.destroy_actor()
    yield from wait(0.15)
    check('T06_destroy_clears', targeting.get_current_target() is None and not targeting.is_component_tick_enabled())
    gm.ensure_fighters_spawned()
    p2 = gm.get_opponent_fighter()
    yield from wait(0.2)
    check('T06_respawn_reassign', len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)) == 2
          and targeting.get_preferred_target() == p2 and targeting.lock_best_target()
          and p2.get_stats_init_count() == 1, pos(p2))
    check('T06_camera_unchanged', abs(pc.get_control_rotation().yaw - yaw) < 1)
    # Repeat while old UObjects have not been garbage collected (name-conflict regression).
    for _ in range(3):
        p2.destroy_actor()
        gm.ensure_fighters_spawned()
        p2 = gm.get_opponent_fighter()
        yield from wait(0.1)
        assert targeting.get_preferred_target() == p2 and targeting.lock_best_target()
    check('T06_repeated_respawn', len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)) == 2,
          p2.get_name())
    p1.reset_to_initial_state()
    p2.reset_to_initial_state()
    pc.set_control_rotation(unreal.Rotator(pitch=-10, yaw=0, roll=0))
    yield from wait(0.2)
    check('T07_final_restore', attributes(p1) == attributes(p2) and p1.get_stats_init_count() == p2.get_stats_init_count() == 1)
    pc.call_method('JJKFighters')


generator = suite()
started = time.monotonic()
def tick(_dt):
    try:
        assert time.monotonic() - started < 150, 'Acceptance watchdog timeout'
        next(generator)
    except StopIteration:
        state['status'] = 'passed'
        finish()
    except Exception:
        state['status'] = 'failed'
        state['error'] = traceback.format_exc()
        unreal.log_error(state['error'])
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    if unreal.SystemLibrary.is_valid(p1):
        p1.set_editor_property('definition', original_definition)
        p1.reset_to_initial_state()
    state['duration_seconds'] = round(time.monotonic() - started, 3)
    write()


write()
handle = unreal.register_slate_post_tick_callback(tick)
print('M1 acceptance started; result:', str(out))



