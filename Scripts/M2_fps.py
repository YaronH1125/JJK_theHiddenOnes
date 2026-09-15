# -*- coding: utf-8 -*-
"""M2-T09: 30/60/120 FPS 下复现快速攻击，验证无漏判/重复。结果写 Saved/M2_fps.json"""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M2_fps.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE not running"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()
DAMAGE = 35.0

state = {"status": "running", "results": []}
out.write_text(json.dumps(state), encoding="utf-8")
unreal.GameplayStatics.set_game_paused(world, False)
perf = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)


def hp(actor):
    return actor.get_fighter_attribute_set().get_editor_property("health").current_value


def place(actor, x, y, z=100):
    actor.character_movement.stop_movement_immediately()
    actor.set_actor_location(unreal.Vector(x, y, z), False, True)


def reset_all():
    gm.reset_training()
    place(p1, 400, 0)
    place(p2, 500, 0)


def attack_light(fighter):
    inp = fighter.get_combat_input()
    inp.notify_attack_pressed()
    inp.notify_attack_released()


gen = None
results = []


def suite():
    for fps in (30, 60, 120):
        unreal.SystemLibrary.execute_console_command(world, f"t.MaxFPS {fps}")
        for _ in wait(0.8):
            yield
        n = 0
        t0 = unreal.GameplayStatics.get_time_seconds(world)
        while unreal.GameplayStatics.get_time_seconds(world) < t0 + 1.0:
            n += 1
            yield
        measured = n
        reset_all()
        for _ in wait(0.3):
            yield
        hp2_0 = hp(p2)
        attack_light(p1)
        for _ in wait(1.4):
            yield
        dealt = hp2_0 - hp(p2)
        ok = abs(dealt - DAMAGE) < 0.01 and p1.get_combat_hit().get_hit_count() == 1
        results.append({"target_fps": fps, "measured_fps": measured,
                        "damage": dealt, "hits": p1.get_combat_hit().get_hit_count(),
                        "passed": bool(ok)})
        out.write_text(json.dumps(state, ensure_ascii=False), encoding="utf-8")
    unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 0")
    state["results"] = results
    state["status"] = "passed" if len(results) == 3 and all(r["passed"] for r in results) else "failed"
    out.write_text(json.dumps(state, ensure_ascii=False), encoding="utf-8")


def wait(seconds):
    end = unreal.GameplayStatics.get_time_seconds(world) + seconds
    while unreal.GameplayStatics.get_time_seconds(world) < end:
        yield


gen = suite()


def tick(_=None):
    global handle
    try:
        next(gen)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)
