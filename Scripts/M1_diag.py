# -*- coding: utf-8 -*-
import unreal
TAG = "[M1Diag]"
def log(m): unreal.log(f"{TAG} {m}")
try:
    uss = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    log(f"USS={uss}")
    w = uss.get_game_world()
    log(f"game_world={w}")
except Exception as e:
    log(f"USS failed: {e}")
try:
    w2 = unreal.EditorLevelLibrary.get_game_world()
    log(f"ELL game_world={w2}")
except Exception as e:
    log(f"ELL failed: {e}")
try:
    log(f"PIE running={unreal.EditorLevelLibrary.is_in_play_in_editor()}")
except Exception as e:
    log(f"ELL pie check failed: {e}")
