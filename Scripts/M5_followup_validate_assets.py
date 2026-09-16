import unreal,json
from pathlib import Path
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
d=fd.get_editor_property('dodge_config')
t=fd.get_editor_property('throw_config')
values={k:fd.get_editor_property(k) for k in ['initial_health','initial_action_resource','initial_cursed_energy','initial_energy','sprint_speed_multiplier']}
values['dodge']=d.export_text();values['throw']=t.export_text()
assert d.dodge_speed==900 and d.dodge_cost==1 and d.cancel_dodge_total_cost==2
assert abs(d.invulnerable_duration-.25)<.0001 and abs(d.recovery_duration-.4)<.0001
assert t.pair_distance==90 and values['initial_health']==1000 and values['initial_action_resource']==5
assert values['sprint_speed_multiplier']==1.5
for name in ['Dodge','Backstep']:
 m=unreal.load_asset('/Game/Training/Movement/AM_'+name)
 assert abs(m.get_play_length()-.9666667)<.01
 anim=m.get_editor_property('slot_anim_tracks')[0].anim_track.anim_segments[0].anim_reference
 assert anim.get_path_name().startswith('/Game/Training/Movement/') and anim.get_editor_property('force_root_lock')
 values[name]={'montage':m.get_path_name(),'length':m.get_play_length(),'pose':anim.get_path_name()}
values['status']='passed'
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
(root/'Docs/开发过程/验收记录/M5_Followup_AssetConfig.json').write_text(json.dumps(values,ensure_ascii=False,indent=2),encoding='utf-8')
print(values)
