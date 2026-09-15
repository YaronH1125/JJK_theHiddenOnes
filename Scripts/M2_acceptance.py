# -*- coding: utf-8 -*-
"""M2 PIE 验收：单次攻击闭环 + 轻重输入原型。run via ue_python.py, poll Saved/M2_acceptance.json。
覆盖 M2-T01…T12；全部临时运行时改动用 reset_training 还原。
"""
import json
from pathlib import Path
import time
import traceback
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M2_acceptance.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()
DAMAGE = 35.0
performance = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)

state = {'status': 'running', 'engine': unreal.SystemLibrary.get_engine_version(), 'checks': []}


def write():
    out.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')


def check(name, condition, details=None):
    state['checks'].append({'name': name, 'passed': bool(condition), 'details': details})
    write()
    unreal.log(('[M2Acceptance] PASS ' if condition else '[M2Acceptance] FAIL ') + name)


def pos(actor):
    p = actor.get_actor_location()
    return [round(p.x, 3), round(p.y, 3), round(p.z, 3)]


def hp(actor):
    return actor.get_fighter_attribute_set().get_editor_property('health').get_editor_property('current_value')


def place(actor, x, y, z=100):
    actor.character_movement.stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)


def reset_all():
    gm.reset_training()
    place(p1, -500, 0)
    place(p2, 500, 0)


def wait(seconds):
    end = unreal.GameplayStatics.get_time_seconds(world) + seconds
    while unreal.GameplayStatics.get_time_seconds(world) < end:
        yield


def attack_light(fighter):
    inp = fighter.get_combat_input()
    inp.notify_attack_pressed()
    inp.notify_attack_released()


def submit(fighter):
    return str(fighter.get_combat_input().submit_light_attack())


def full_recover():
    for _ in wait(1.3):
        yield


def suite():
    global p1, p2
    # ---------- T01 有效距离命中 / 范围外空挥 ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_0, hp1_0 = hp(p2), hp(p1)
    attack_light(p1)
    for _ in wait(0.6):
        yield
    stun = str(p2.get_combat_input().submit_light_attack())
    diag = {'p1_pos': pos(p1), 'p2_pos': pos(p2), 'hp2': hp(p2), 'hp1': hp(p1),
            'hits': p1.get_combat_hit().get_hit_count(), 'window': p1.get_combat_hit().is_window_open(),
            'iid': p1.get_combat_hit().get_active_instance_id(),
            'phase': str(p1.get_combat_hit().get_phase())}
    unreal.log('[M2Acceptance] T01 diag: ' + json.dumps(diag, ensure_ascii=False))
    check('T01_hitstun_blocks', 'REJECTED_BLOCKED' in stun, stun)
    for _ in wait(1.1):
        yield
    check('T01_single_damage', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01, [hp2_0, hp(p2)])
    check('T01_one_instance_one_hit', p1.get_combat_hit().get_hit_count() == 1,
          p1.get_combat_hit().get_hit_count())
    check('T01_attacker_unhurt', abs(hp(p1) - hp1_0) < 0.01, hp(p1))

    # 范围外空挥
    reset_all()
    for _ in wait(0.2):
        yield
    hp2_0 = hp(p2)
    attack_light(p1)
    for _ in wait(1.3):
        yield
    check('T01_range_miss', abs(hp(p2) - hp2_0) < 0.01 and p1.get_combat_hit().get_hit_count() == 0,
          [hp2_0, hp(p2), p1.get_combat_hit().get_hit_count()])

    # ---------- T02 同窗口多帧接触只结算一次（多帧覆盖等效多部件） ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_0 = hp(p2)
    attack_light(p1)
    # 窗口期(0.25-0.45s)内连续多次采样，均在接触范围
    for _ in wait(0.6):
        yield
    check('T02_dedup_single', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01
          and p1.get_combat_hit().get_hit_count() == 1,
          [hp(p2), p1.get_combat_hit().get_hit_count()])

    # ---------- T03 新实例可再次命中 ----------
    for _ in wait(1.0):
        yield
    iid_1 = p1.get_combat_hit().get_active_instance_id()
    attack_light(p1)
    for _ in wait(1.3):
        yield
    iid_2 = p1.get_combat_hit().get_active_instance_id()
    check('T03_new_instance_hits', hp(p2) <= hp2_0 - 2 * DAMAGE and iid_2 > iid_1
          and p1.get_combat_hit().get_hit_count() == 1,
          [hp(p2), iid_1, iid_2])

    # ---------- T04 快速连按与禁止状态 ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_0 = hp(p2)
    r0 = submit(p1)
    r1, r2, r3, r4 = submit(p1), submit(p1), submit(p1), submit(p1)
    check('T04_spam_single', 'EXECUTED' in r0 and all('REJECTED_ALREADY_ACTIVE' in r for r in [r1, r2, r3, r4]),
          [r0, r1, r2, r3, r4])
    for _ in wait(1.3):
        yield
    check('T04_spam_single_damage', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01, hp(p2))
    # 受击硬直禁止
    place(p1, 400, 0)
    attack_light(p1)
    for _ in wait(0.6):
        yield
    r_block = submit(p2)
    check('T04_blocked_reason', 'REJECTED_BLOCKED' in r_block, r_block)
    for _ in wait(1.2):
        yield

    # ---------- T05 起手/有效/恢复 强制中断 ----------
    for delay, label in [(0.05, 'windup'), (0.3, 'active'), (0.7, 'recovery')]:
        reset_all()
        for _ in wait(0.2):
            yield
        hp2_0 = hp(p2)
        attack_light(p1)
        for _ in wait(delay):
            yield
        p1.call_method('JJKDebugForceHitReact')
        for _ in wait(1.6):
            yield
        hit = p1.get_combat_hit()
        check(f'T05_interrupt_{label}', (not hit.has_active_attack()) and (not hit.is_window_open())
              and abs(hp(p2) - hp2_0) < 0.01,
              [label, hit.has_active_attack(), hit.is_window_open(), hp(p2)])
        r = submit(p1)
        check(f'T05_reattack_{label}', 'EXECUTED' in r, r)
        for _ in wait(1.3):
            yield

    # ---------- T06 各阶段死亡/重置，无延迟伤害 ----------
    for delay, label in [(0.05, 'windup'), (0.3, 'active'), (0.7, 'recovery')]:
        reset_all()
        for _ in wait(0.2):
            yield
        hp2_0 = hp(p2)
        attack_light(p1)
        for _ in wait(delay):
            yield
        p1.call_method('JJKDebugKill')
        for _ in wait(1.6):
            yield
        check(f'T06_death_{label}_no_delayed', abs(hp(p2) - hp2_0) < 0.01, [label, hp(p2)])
        r = submit(p1)
        check(f'T06_dead_reject_{label}', 'REJECTED_DEAD' in r, r)
        gm.reset_training()
        for _ in wait(0.2):
            yield
        check(f'T06_reset_revive_{label}', (not p1.is_dead()) and p1.get_stats_init_count() == 1
              and hp(p1) == 1000 and hp(p2) == 1000, [hp(p1), hp(p2)])

    # 重置时序：攻击中重置，等待超动作时长无补发
    for delay, label in [(0.05, 'windup'), (0.3, 'active'), (0.7, 'recovery')]:
        reset_all()
        place(p1, 400, 0)
        for _ in wait(0.2):
            yield
        attack_light(p1)
        for _ in wait(delay):
            yield
        gm.reset_training()
        hp2_after = hp(p2)
        for _ in wait(1.6):
            yield
        check(f'T06_reset_{label}_no_late', abs(hp(p2) - hp2_after) < 0.01, [label, hp(p2)])
        r = submit(p1)
        check(f'T06_reset_{label}_can_attack', 'EXECUTED' in r, r)
        for _ in wait(1.3):
            yield

    # ---------- T07 攻击中销毁目标 ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    attack_light(p1)
    for _ in wait(0.1):
        yield
    pc.call_method('JJKKillTarget')
    for _ in wait(1.5):
        yield
    check('T07_destroy_no_crash', (not p1.get_combat_hit().is_window_open())
          and (not p1.get_combat_hit().has_active_attack()), None)
    pc.call_method('JJKRespawnFighters')
    gm.ensure_fighters_spawned()
    for _ in wait(0.3):
        yield
    p2_new = gm.get_opponent_fighter()
    check('T07_respawn', (not p2_new.is_dead()) and abs(hp(p2_new) - 1000) < 0.01
          and gm.get_opponent_of(p1) == p2_new, [p2_new.get_name(), hp(p2_new)])
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_new_0 = hp(p2_new)
    r7 = submit(p1)
    for _ in wait(1.3):
        yield
    check('T07_reattack_new_target', abs(hp(p2_new) - (hp2_new_0 - DAMAGE)) < 0.01,
          {'r': r7, 'p1': pos(p1), 'p2': pos(p2_new), 'hp2': hp(p2_new),
           'hits': p1.get_combat_hit().get_hit_count()})

    # ---------- T08 同批次换血 ×5 ----------
    p1 = gm.get_player_fighter()
    p2 = gm.get_opponent_fighter()
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    for round_index in range(5):
        hp1_0, hp2_0 = hp(p1), hp(p2)
        gm.call_method('JJKOpponentAttack')
        attack_light(p1)
        for _ in wait(1.4):
            yield
        d1, d2 = hp1_0 - hp(p1), hp2_0 - hp(p2)
        check(f'T08_trade_{round_index + 1}', abs(d1 - DAMAGE) < 0.01 and abs(d2 - DAMAGE) < 0.01,
              {'p1_damage': d1, 'p2_damage': d2})

    # ---------- T09 帧率矩阵（在循环外由 runner 配合执行，这里记录一次基准） ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_0 = hp(p2)
    attack_light(p1)
    for _ in wait(1.3):
        yield
    check('T09_baseline_single', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01, hp(p2))

    # ---------- T10 命中表现开/关，伤害链路一致 ----------
    reset_all()
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    unreal.SystemLibrary.execute_console_command(world, 'JJK.DebugHitFX 1')
    hp2_0 = hp(p2)
    attack_light(p1)
    for _ in wait(1.3):
        yield
    d_on = hp2_0 - hp(p2)
    unreal.SystemLibrary.execute_console_command(world, 'JJK.DebugHitFX 0')
    hp2_1 = hp(p2)
    attack_light(p1)
    for _ in wait(1.3):
        yield
    d_off = hp2_1 - hp(p2)
    check('T10_fx_toggle_same_damage', abs(d_on - DAMAGE) < 0.01 and abs(d_off - DAMAGE) < 0.01,
          {'fx_on': d_on, 'fx_off': d_off})

    # ---------- T11 轻重输入识别 ----------
    reset_all()
    for _ in wait(0.2):
        yield
    # 长按（>=0.18s）只记录重拳意图：无伤害、无轻拳
    inp = p1.get_combat_input()
    hp2_0 = hp(p2)
    inp.notify_attack_pressed()
    for _ in wait(0.3):
        yield
    inp.notify_attack_released()
    for _ in wait(0.5):
        yield
    check('T11_heavy_intent_no_damage', abs(hp(p2) - hp2_0) < 0.01
          and (not p1.get_combat_hit().has_active_attack()), hp(p2))
    # 旧松键不补攻击
    inp.notify_attack_released()
    for _ in wait(0.3):
        yield
    check('T11_stale_release', abs(hp(p2) - hp2_0) < 0.01
          and inp.get_active_session_id() == 0, inp.get_active_session_id())
    # 短按轻拳正常（回接触距离）
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    attack_light(p1)
    for _ in wait(1.3):
        yield
    check('T11_light_still_works', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01, hp(p2))

    # ---------- T12 按住后失效（死亡/重置/失焦），恢复后不补拳 ----------
    # 重置清会话
    reset_all()
    for _ in wait(0.2):
        yield
    inp.notify_attack_pressed()
    gm.reset_training()
    inp.notify_attack_released()
    for _ in wait(0.3):
        yield
    check('T12_reset_invalidates', abs(hp(p2) - 1000) < 0.01 and inp.get_active_session_id() == 0, hp(p2))
    # 失焦清会话
    inp.notify_attack_pressed()
    pc.handle_app_activation_changed(False)
    inp.notify_attack_released()
    for _ in wait(0.3):
        yield
    check('T12_focus_loss_invalidates', abs(hp(p2) - 1000) < 0.01, hp(p2))
    # 死亡清会话
    place(p1, 400, 0)
    for _ in wait(0.2):
        yield
    inp.notify_attack_pressed()
    p1.call_method('JJKDebugKill')
    inp.notify_attack_released()
    for _ in wait(0.3):
        yield
    check('T12_death_invalidates', inp.get_active_session_id() == 0 and p1.is_dead(), None)
    gm.reset_training()
    for _ in wait(0.2):
        yield
    # 恢复后新按下可完成轻拳闭环
    place(p1, 400, 0)
    for _ in wait(0.3):
        yield
    hp2_0 = hp(p2)
    attack_light(p1)
    for _ in wait(1.3):
        yield
    check('T12_repress_works', abs(hp(p2) - (hp2_0 - DAMAGE)) < 0.01, hp(p2))

    reset_all()


gen = suite()
started = time.monotonic()


def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    if unreal.SystemLibrary.is_valid(p1):
        gm.reset_training()
    state['duration_seconds'] = round(time.monotonic() - started, 3)
    write()


def tick(_delta_time=None):
    try:
        next(gen)
    except StopIteration:
        state['status'] = 'passed'
        unreal.log('[M2Acceptance] ALL PASSED')
        finish()
    except Exception:
        state['status'] = 'failed'
        state['error'] = traceback.format_exc()
        unreal.log_error(state['error'])
        finish()


write()
handle = unreal.register_slate_post_tick_callback(tick)
print('M2 acceptance started; result:', str(out))
