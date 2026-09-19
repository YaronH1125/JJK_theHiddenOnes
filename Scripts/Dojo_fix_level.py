"""Idempotent project-layer repair. Never saves Mishima_DOJO packages."""
import unreal
es=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert es.load_level('/Game/Maps/L_DojoArena')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
walls=[a for a in actors if a.get_path_name().startswith('/Game/Maps/L_DojoArena.') and a.get_actor_label() in ('VFXWall_N','VFXWall_S','VFXWall_E','VFXWall_W')]
assert len(walls)==4
for a in walls:
    a.modify()
    a.set_actor_hidden_in_game(True)
    c=a.static_mesh_component
    c.modify()
    c.set_visibility(False)
    c.set_hidden_in_game(True)
    c.set_cast_shadow(False)
    # Bounds are gameplay-only. A camera must not retract against invisible walls.
    c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
assert es.save_current_level()
bp=unreal.load_asset('/Game/Training/BP_DojoTrainingGameMode')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
cdo.modify()
# Actual inside wall faces: X[-832,776], Y[-652,652].
cdo.set_editor_property('arena_center',unreal.Vector(-28,0,0))
cdo.set_editor_property('arena_walkable_half_extent',unreal.Vector2D(804,652))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
extent=unreal.get_default_object(bp.generated_class()).get_editor_property('arena_walkable_half_extent')
assert extent.x==804 and extent.y==652, str(extent)
print('Saved rectangle:',extent)
print('DOJO project-layer repair saved; source art untouched')
