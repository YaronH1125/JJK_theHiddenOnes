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
log(f"PIE running={w is not None}")
if w:
    fighters = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.FighterCharacter)
    log(f"fighter_count={len(fighters)}")
    for fighter in fighters:
        log(f"{fighter.get_name()} position={fighter.get_actor_location()} init_count={fighter.get_stats_init_count()}")
