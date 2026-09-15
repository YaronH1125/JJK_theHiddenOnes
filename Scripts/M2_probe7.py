# -*- coding: utf-8 -*-
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'M2_probe7.json'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE not running"
st = {"times": [], "status": "running"}
out.write_text(json.dumps(st), encoding="utf-8")
n = [0]


def tick(_=None):
    n[0] += 1
    st["times"].append(round(unreal.GameplayStatics.get_time_seconds(world), 3))
    if n[0] >= 20:
        st["status"] = "done"
        
        out.write_text(json.dumps(st), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)
