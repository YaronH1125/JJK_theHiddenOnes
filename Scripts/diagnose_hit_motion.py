"""Transient PIE experiments; records both fighters and restores runtime state.

Load as a module and call start() through ue_python.py. No assets are saved.
"""
import json
import time
import traceback
from pathlib import Path
import unreal


def vec(v):
    return [v.x, v.y, v.z]


def snapshot(a):
    mesh = a.mesh
    anim = mesh.get_anim_instance()
    montage = anim.get_current_active_montage()
    return {
        'actor': vec(a.get_actor_location()),
        'velocity': vec(a.get_velocity()),
        'mesh_relative': vec(mesh.get_editor_property('relative_location')),
        'mode': str(a.character_movement.get_editor_property('movement_mode')),
        'health': a.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value'),
        'montage': montage.get_name() if montage else None,
        'montage_time': anim.montage_get_position(montage) if montage else None,
        'bones': {b: vec(mesh.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT).translation)
                  for b in ('root', 'pelvis', 'head', 'foot_l', 'foot_r', 'ik_foot_l', 'ik_foot_r')},
    }


def start(disable_control_rig=False, healthy_source_pose=False):
    global world, gm, p1, p2, controller, was_active, originals, perf, throttle, report, output, runner, handle, started, old_rig_cvar, test_sequence
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'PIE is required'
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    gm = pc.get_training_game_mode()
    p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()
    # Fail immediately if any sampling API is unavailable.
    snapshot(p1)
    snapshot(p2)
    originals = [(a, a.get_actor_location(), a.get_actor_rotation()) for a in (p1, p2)]
    controller = p2.get_controller()
    was_active = controller.is_ai_active() if controller else False
    perf = unreal.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
    throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
    perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
    if controller:
        controller.deactivate_ai()
    old_rig_cvar = unreal.SystemLibrary.get_console_variable_int_value('ControlRig.DisableExecutionInAnimNode')
    if disable_control_rig:
        unreal.SystemLibrary.execute_console_command(world, 'ControlRig.DisableExecutionInAnimNode 1')
    test_sequence = None
    if healthy_source_pose:
        test_sequence = unreal.load_asset('/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01')
        assert test_sequence.get_retarget_source_asset() is None, 'Expected no pre-existing source mesh'
        source_mesh = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
        assert source_mesh is not None, 'Manny source mesh is missing'
        test_sequence.set_retarget_source_asset(source_mesh)
        test_sequence.update_retarget_source_asset_data()
    filename = 'hit_motion_healthy_source.json' if healthy_source_pose else ('hit_motion_no_controlrig.json' if disable_control_rig else 'hit_motion_experiments.json')
    output = Path(unreal.Paths.project_saved_dir()) / filename
    report = {'status': 'running', 'healthy_source_pose': healthy_source_pose, 'disable_control_rig': disable_control_rig, 'original_rig_cvar': old_rig_cvar, 'cases': [], 'errors': []}
    started = time.monotonic()
    runner = suite()
    handle = unreal.register_slate_post_tick_callback(tick)
    write()


def write():
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')


def wait(seconds, case=None):
    end = unreal.GameplayStatics.get_time_seconds(world) + seconds
    while unreal.GameplayStatics.get_time_seconds(world) < end:
        if case is not None:
            case['samples'].append({'t': unreal.GameplayStatics.get_time_seconds(world), 'p1': snapshot(p1), 'p2': snapshot(p2)})
        yield


def suite():
    for name, distance, injected in [('isolated_reaction', 1000, True), ('normal_attack_close', 130, False), ('normal_attack_lunge', 260, False)]:
        gm.reset_training()
        if controller:
            controller.deactivate_ai()
        for a, x, yaw in [(p1, 500-distance, 0), (p2, 500, 180)]:
            a.character_movement.stop_movement_immediately()
            a.set_actor_location(unreal.Vector(x, 0, 98.15), False, True)
            a.set_actor_rotation(unreal.Rotator(0, yaw, 0), True)
        case = {'name': name, 'samples': []}
        report['cases'].append(case)
        yield from wait(.5)
        yield from wait(.2, case)
        case['trigger_time'] = unreal.GameplayStatics.get_time_seconds(world)
        if injected:
            p2.call_method('JJKDebugForceHitReact')
        else:
            case['request_result'] = str(p1.get_combat_input().submit_light_attack())
        yield from wait(1.5, case)
        write()


def cleanup():
    if test_sequence is not None:
        test_sequence.clear_retarget_source_asset()
        test_sequence.update_retarget_source_asset_data()
    unreal.SystemLibrary.execute_console_command(world, 'ControlRig.DisableExecutionInAnimNode ' + str(old_rig_cvar))
    gm.reset_training()
    for a, location, rotation in originals:
        a.set_actor_location(location, False, True)
        a.set_actor_rotation(rotation, True)
    if controller and was_active:
        controller.activate_ai(p1)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', throttle)


def tick(dt):
    try:
        if time.monotonic() - started > 60:
            raise TimeoutError('No completed experiment within 60 seconds')
        next(runner)
        return
    except StopIteration:
        report['status'] = 'complete'
    except Exception:
        report['status'] = 'failed'
        report['errors'].append(traceback.format_exc())
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        cleanup()
    except Exception:
        report['errors'].append(traceback.format_exc())
    write()
    unreal.log('HIT_MOTION_DIAG ' + report['status'])
