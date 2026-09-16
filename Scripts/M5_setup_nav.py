"""幂等设置白盒 NavMesh：代理与角色 Capsule 匹配，固定池防止小地图估算容量不足。"""
import unreal
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
volumes=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.NavMeshBoundsVolume)]
if not volumes:
 volumes=[actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume,unreal.Vector(0,0,100))]
for v in volumes:
 v.set_actor_scale3d(unreal.Vector(13,13,5))
 v.set_actor_label('ArenaNavBounds')
for a in actors.get_all_level_actors():
 if isinstance(a,unreal.RecastNavMesh):
  for k,v in dict(fixed_tile_pool_size=True,tile_pool_size=1024,runtime_generation=unreal.RuntimeGenerationType.DYNAMIC).items():
   a.set_editor_property(k,v)
  unreal.log('M5 Nav config: radius=42 height=192 pool=1024 dynamic')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,False)
