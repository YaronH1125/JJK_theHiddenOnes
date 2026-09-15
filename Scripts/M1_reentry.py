"""Assert a fresh PIE's saved configuration, append to reentry evidence."""
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and world.get_name() == 'L_TrainingArena'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
fighters = [gm.get_player_fighter(), gm.get_opponent_fighter()]
assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)) == 2
assert pc.get_class().get_name() == 'BP_ArenaPlayerController_C'
record = {'world': world.get_path_name(), 'fighters': []}
for f, x, yaw in zip(fighters, [-500, 500], [0, 180]):
    p = f.get_actor_location()
    assert abs(p.x-x) < 0.1 and abs(p.y) < 0.1 and 96 <= p.z <= 101
    assert abs(abs(f.get_actor_rotation().yaw) - yaw) < 0.1
    assert f.get_stats_init_count() == 1
    vals = unreal.get_default_object(unreal.AbilitySystemInspectorToolset).call_method('GetAttributeValues', (f,))
    stats = {v.attribute_name:v.current_value for v in vals}
    assert stats['Health'] == 1000 and stats['ActionResource'] == 100 and stats['Energy'] == 0
    record['fighters'].append({'name': f.get_name(), 'position': [p.x,p.y,p.z], 'initialization_count':f.get_stats_init_count(), 'stats':stats})
assert fighters[0].get_controller() == pc and fighters[1].get_controller() is None
assert fighters[0].get_targeting().get_preferred_target() == fighters[1]
pc.call_method('JJKFighters')
out = Path(unreal.Paths.project_saved_dir()) / 'M1_reentry.json'
entries = json.loads(out.read_text(encoding='utf-8')) if out.exists() else []
entries.append(record)
out.write_text(json.dumps(entries, ensure_ascii=False, indent=2), encoding='utf-8')
print('M1_REENTRY_PASS', record)
