"""Create explicit placeholder dodge poses; do not modify template animation assets."""
import unreal
root='/Game/Training/Movement'
unreal.EditorAssetLibrary.make_directory(root)
def duplicate(source,name):
 target=root+'/'+name
 return unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else unreal.EditorAssetLibrary.duplicate_asset(source,target)
def montage(source,name):
 anim=duplicate(source,'A_'+name)
 anim.set_editor_property('enable_root_motion',False)
 anim.set_editor_property('force_root_lock',True)
 m=duplicate('/Game/Variant_Platforming/Anims/AM_Dash','AM_'+name)
 tracks=m.get_editor_property('slot_anim_tracks')
 track=tracks[0].get_editor_property('anim_track');segments=track.get_editor_property('anim_segments')
 seg=segments[0];seg.set_editor_property('anim_reference',anim);seg.set_editor_property('anim_start_time',0.)
 seg.set_editor_property('cached_play_length',anim.get_play_length());seg.set_editor_property('anim_end_time',min(.9666667,anim.get_play_length()));seg.set_editor_property('anim_play_rate',1.)
 track.set_editor_property('anim_segments',[seg]);slot=tracks[0];slot.set_editor_property('anim_track',track);tracks[0]=slot
 m.set_editor_property('slot_anim_tracks',tracks)
 assert m.get_editor_property('slot_anim_tracks')[0].anim_track.anim_segments[0].anim_reference == anim
 for prop,value in [('blend_in',.05),('blend_out',.1)]:
  blend=m.get_editor_property(prop);blend.set_editor_property('blend_time',value);m.set_editor_property(prop,blend)
 editor=unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
 editor.open_editor_for_assets([m])
 assert abs(m.get_play_length()-.9666667)<.01
 # Nested struct edits may not mark an existing package dirty; persist explicitly.
 unreal.EditorAssetLibrary.save_loaded_asset(anim,only_if_is_dirty=False)
 unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
 editor.close_all_editors_for_asset(m)
 return m
forward=montage('/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash','Dodge')
back=montage('/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Bwd','Backstep')
for path in unreal.EditorAssetLibrary.list_assets('/Game/Training',recursive=True):
 obj=unreal.load_asset(path)
 if isinstance(obj,unreal.FighterDefinition):
  obj.set_editor_property('dodge_montage',forward);obj.set_editor_property('backstep_montage',back)
  unreal.EditorAssetLibrary.save_loaded_asset(obj,only_if_is_dirty=False)
print('Configured forward dodge/backstep poses on fighter definitions')
