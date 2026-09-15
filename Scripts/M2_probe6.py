# -*- coding: utf-8 -*-
"""窗口时机探测 v2：禁节流，逐帧记录 hand 距离与窗口。结果写 Saved/M2_probe6.json"""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M2_probe6.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE not running"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()

perf = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)

gm.reset_training()
p1.character_movement.stop_movement_immediately()
p1.set_actor_location(unreal.Vector(420, 0, 100), False, True)
p2.character_movement.stop_movement_immediately()
p2.set_actor_location(unreal.Vector(500, 0, 100), False, True)

state = {"phase": 0, "t": 0, "min_dist": 1e9, "frames": [], "status": "running"}
out.write_text(json.dumps(state), encoding="utf-8")


def tick(_=None):
    state["t"] += 1
    t = state["t"]
    if state["phase"] == 0:
        if t > 10:
            state["phase"] = 1
            state["t"] = 0
            p1.get_combat_input().notify_attack_pressed()
            p1.get_combat_input().notify_attack_released()
        return
    hit = p1.get_combat_hit()
    hand = p1.mesh.get_socket_location("hand_r")
    d = (hand - p2.get_actor_location()).length()
    state["min_dist"] = min(state["min_dist"], d)
    hp2 = p2.get_fighter_attribute_set().get_editor_property("health").current_value
    state["frames"].append([t, round(d, 1), hit.is_window_open(), hit.get_hit_count(), round(hp2)])
    out.write_text(json.dumps(state), encoding="utf-8")
    if t > 80:
        state["status"] = "done"
        state["min_dist"] = round(state["min_dist"], 1)
        out.write_text(json.dumps(state), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)
