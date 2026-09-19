"""Use the original dojo's walls; remove the obsolete central-platform barriers.
Only the project map and child GameMode are saved. Source art stays read-only.
"""
import unreal
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
es=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert es.load_level('/Game/Maps/L_DojoArena')
actor_system=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=actor_system.get_all_level_actors()
walls=[a for a in actors if a.get_path_name().startswith('/Game/Maps/L_DojoArena.') and a.get_actor_label() in ('VFXWall_N','VFXWall_S','VFXWall_E','VFXWall_W')]
for a in walls:
    assert actor_system.destroy_actor(a)
# Broad navigation envelope outside the original building walls. The real mesh
# collision and connected NavMesh define walkable ground, not this rectangle.
center=unreal.Vector(1250,0,200)
half_extent=unreal.Vector(2550,1700,400)
navs=[a for a in actors if isinstance(a,unreal.NavMeshBoundsVolume) and a.get_path_name().startswith('/Game/Maps/L_DojoArena.')]
assert len(navs)==1
nav=navs[0];nav.modify()
old_extent=nav.get_actor_bounds(False)[1];scale=nav.get_actor_scale3d()
nav.set_actor_scale3d(unreal.Vector(scale.x*half_extent.x/old_extent.x,scale.y*half_extent.y/old_extent.y,scale.z*half_extent.z/old_extent.z))
nav.set_actor_location(center,False,False)
assert es.save_current_level()
bp=unreal.load_asset('/Game/Training/BP_DojoTrainingGameMode')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
cdo.modify()
cdo.set_editor_property('arena_center',unreal.Vector(center.x,center.y,0))
cdo.set_editor_property('arena_walkable_half_extent',unreal.Vector2D(half_extent.x,half_extent.y))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
extent=unreal.get_default_object(bp.generated_class()).get_editor_property('arena_walkable_half_extent')
assert extent.x==half_extent.x and extent.y==half_extent.y, str(extent)
print('Saved rectangle:',extent)
print('Removed central barriers:',len(walls))
print('DOJO uses original wall collision; source art untouched')
