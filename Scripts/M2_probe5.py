# -*- coding: utf-8 -*-
"""探测拳击窗口期 hand_r 轨迹与接触距离。结果写 Saved/M2_probe5.json"""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M2_probe5.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE not running"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
p1, p2 = gm.get_player_fighter(), gm.get_opponent_fighter()

gm.reset_training()
p1.character_movement.stop_movement_immediately()
p1.set_actor_location(unreal.Vector(400, 0, 100), False, True)
p2.character_movement.stop_movement_immediately()
p2.set_actor_location(unreal.Vector(500, 0, 100), False, True)

state = {"phase": 0, "t": 0, "min_dist": 1e9, "frames": [], "status": "running"}
out.write_text(json.dumps(state), encoding="utf-8")
hp2_0 = p2.get_fighter_attribute_set().get_editor_property("health").current_value


def finish():
    state["min_dist"] = round(state["min_dist"], 1)
    state["hp2_0"] = hp2_0
    state["hp2"] = hp2_now()
    state["status"] = "done"
    out.write_text(json.dumps(state), encoding="utf-8")
    unreal.unregister_slate_post_tick_callback(handle)


def hp2_now():
    return p2.get_fighter_attribute_set().get_editor_property("health").current_value


def tick(_=None):
    state["t"] += 1
    t = state["t"]
    if state["phase"] == 0:
        if t > 15:
            state["phase"] = 1
            state["t"] = 0
            p1.get_combat_input().notify_attack_pressed()
            p1.get_combat_input().notify_attack_released()
        return
    hit = p1.get_combat_hit()
    hand = p1.mesh.get_socket_location("hand_r")
    p2c = p2.get_actor_location()
    d = (hand - p2c).length()
    state["min_dist"] = min(state["min_dist"], d)
    hp2 = p2.get_fighter_attribute_set().get_editor_property("health").current_value
    state["frames"].append([t, round(d), hit.is_window_open(), hit.get_hit_count(), round(hp2)])
    out.write_text(json.dumps(state), encoding="utf-8")
    if t > 75:
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
