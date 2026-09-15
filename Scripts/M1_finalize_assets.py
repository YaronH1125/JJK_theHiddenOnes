"""Repair M1 assets in place, compile and reacquire CDOs, then save and assert.
Run through ue_python.py with PIE stopped. Does not rebuild or empty the map.
"""
import math
import unreal

eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert sub.get_game_world() is None, 'Stop PIE before editing assets'
world = sub.get_editor_world()
assert world.get_name() == 'L_TrainingArena', world.get_path_name()
marker_material = unreal.load_asset('/Game/Training/M_FighterTint')
marker_material.set_editor_property('used_with_skeletal_mesh', True)
unreal.MaterialEditingLibrary.recompile_material(marker_material)
assert eal.save_loaded_asset(marker_material, only_if_is_dirty=False)

for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    label = actor.get_actor_label()
    if label == 'ArenaFloor':
        actor.set_actor_scale3d(unreal.Vector(24, 24, 1))
    elif label.startswith('ArenaWall_'):
        angle = int(label.rsplit('_', 1)[1]) * 30
        actor.set_actor_location(unreal.Vector(1150 * math.cos(math.radians(angle)),
                                               1150 * math.sin(math.radians(angle)), 125), False, False)
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=angle + 90, roll=0), False)
        actor.set_actor_scale3d(unreal.Vector((2300 * math.tan(math.pi / 12) + 30) / 100, 0.5, 2.5))
    if label.startswith(('ArenaFloor', 'ArenaWall_')):
        actor.static_mesh_component.set_collision_profile_name('BlockAll')
    if label.startswith('PlayerStart_'):
        actor.get_editor_property('root_component').set_collision_profile_name('NoCollision')

fighter = unreal.load_asset('/Game/Training/BP_Fighter')
pc = unreal.load_asset('/Game/Training/BP_ArenaPlayerController')
gm = unreal.load_asset('/Game/Training/BP_TrainingGameMode')
template = unreal.get_default_object(unreal.load_class(None, '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C'))
for bp in [fighter, pc, gm]:
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

cdo = unreal.get_default_object(fighter.generated_class())
mesh = cdo.get_editor_property('mesh')
source = template.get_editor_property('mesh')
mesh.set_editor_property('relative_location', source.get_editor_property('relative_location'))
mesh.set_editor_property('relative_rotation', source.get_editor_property('relative_rotation'))
mesh.set_editor_property('relative_scale3d', source.get_editor_property('relative_scale3d'))
cdo.get_editor_property('character_movement').set_editor_property('run_physics_with_no_controller', True)

# Template mouse look uses a separate mapping context in this engine version.
imc = unreal.load_asset('/Game/Training/IMC_Training')
mapping_data = imc.get_editor_property('default_key_mappings')
mappings = list(mapping_data.get_editor_property('mappings'))
mouse_action = cdo.get_editor_property('mouse_look_action')
if not any(m.get_editor_property('action') == mouse_action for m in mappings):
    mouse_imc = unreal.load_asset('/Game/Input/IMC_MouseLook')
    assert mouse_imc, 'Template mouse-look context missing'
    mappings.extend(list(mouse_imc.get_editor_property('default_key_mappings').get_editor_property('mappings')))
for path, key_name in [('IA_LockTarget', 'MiddleMouseButton'), ('IA_RecenterCamera', 'C')]:
    action = unreal.load_asset('/Game/Training/' + path)
    if not any(m.action == action for m in mappings):
        key = unreal.Key()
        key.set_editor_property('key_name', key_name)
        mappings.append(unreal.EnhancedActionKeyMapping(action=action, key=key))
mapping_data.set_editor_property('mappings', mappings)
imc.set_editor_property('default_key_mappings', mapping_data)
assert eal.save_loaded_asset(imc, only_if_is_dirty=False)

gm_cdo = unreal.get_default_object(gm.generated_class())
gm_cdo.set_editor_property('player_controller_class', pc.generated_class())
gm_cdo.set_editor_property('fighter_class', fighter.generated_class())
gm_cdo.set_editor_property('default_pawn_class', None)
for bp in [fighter, pc, gm]:
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert eal.save_loaded_asset(bp, only_if_is_dirty=False), bp.get_path_name()
# Do not retain generated-class/CDO references across compilation.
assert unreal.get_default_object(gm.generated_class()).get_editor_property('player_controller_class') == pc.generated_class()
assert unreal.get_default_object(gm.generated_class()).get_editor_property('fighter_class') == fighter.generated_class()
assert unreal.get_default_object(fighter.generated_class()).get_editor_property('mesh').get_editor_property('relative_location').z < -80
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('M1_ASSETS_VERIFIED', [(str(m.action), str(m.key)) for m in mappings])
