# -*- coding: utf-8 -*-
"""M3 幂等落盘：白盒拳脚共用已验证的拳击/受击 Montage，仅验证规则。"""
import unreal
from pathlib import Path
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
EAL=unreal.EditorAssetLibrary
AT=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/Training/'
attack=EAL.load_asset(folder+'AM_M2_A1')
react=EAL.load_asset(folder+'AM_M2_HitReact')
assert attack.get_editor_property('sequence_length') > .9
defs=[]
specs=[('A1',35,0,True,False,False),('A2',40,0,False,False,False),
       ('A3',55,450,False,False,False),('HeavyPunch',90,600,False,False,False),
       ('Kick',45,0,False,False,False),('HeavyKick',100,450,False,True,False)]
for idx,(name,damage,kb,throw,kd,armor) in enumerate(specs):
    path=folder+'DA_M3_'+name
    da=EAL.load_asset(path) if EAL.does_asset_exist(path) else None
    if da is None:
        factory=unreal.DataAssetFactory();factory.set_editor_property('DataAssetClass',unreal.AttackDefinition)
        da=AT.create_asset('DA_M3_'+name,folder.rstrip('/'),unreal.AttackDefinition,factory)
    props=dict(display_name=unreal.Text(name+'（M3 拳击动画占位）'),attack_type_name='Kick' if 'Kick' in name else 'Punch',
               segment_id=idx,damage=float(damage),montage=attack,hit_react_montage=react,trace_socket='hand_r',
               trace_radius=30.0 if 'Kick' in name else 20.0,hit_stun_duration=.5,
               bWindowFromAnimNotifies=False,window_start_time=.25,window_end_time=.45,
               knockback_strength=float(kb),bKnockdown=kd,bGrantsSuperArmor=armor,bCanThrow=throw,
               bBlockable=True,bDodgeable=True,guard_stun_duration=.35,
               combo_window_start_time=.55,combo_window_end_time=.85,
               bAllowNextSegment=idx<2,bAllowHeavyTransition=idx==1,bAllowKickTransition=idx==1,
               cancel_window_start_time=.1,cancel_window_end_time=.8,interrupt_level=2 if kd else 1,armor_resistance_level=1)
    for k,v in props.items():da.set_editor_property(k,v)
    assert EAL.save_loaded_asset(da,False)
    defs.append(da)
fighter=EAL.load_asset(folder+'DA_Fighter_Ishigori')
for k,v in dict(attack_definition=defs[0],combo_segments=defs[:3],heavy_punch_definition=defs[3],
               kick_definition=defs[4],heavy_kick_definition=defs[5],combo_cache_lifetime=.5,
               max_cursed_energy=100.,initial_cursed_energy=100.,melee_cursed_energy_gain=3.,
               dodge_ability=unreal.DodgeAbility.static_class(),stance_switch_ability=unreal.StanceSwitchAbility.static_class()).items():
    fighter.set_editor_property(k,v)
assert EAL.save_loaded_asset(fighter,False)
bp=EAL.load_asset(folder+'BP_ArenaPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
imc=EAL.load_asset(folder+'IMC_Training');data=imc.get_editor_property('default_key_mappings')
specs=[('Dodge','LeftShift','dodge_action'),('Guard','F','guard_action'),('Kick','Q','kick_action'),('StanceSwitch','E','stance_switch_action')]
keys={x[1] for x in specs}
mappings=[m for m in data.get_editor_property('mappings') if str(m.key.get_editor_property('key_name')) not in keys]
for name,key_name,prop in specs:
    path=folder+'IA_'+name
    ia=EAL.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset('IA_'+name,folder.rstrip('/'),unreal.InputAction,unreal.InputAction_Factory())
    ia.set_editor_property('value_type',unreal.InputActionValueType.BOOLEAN)
    ia.set_editor_property('triggers',[])
    assert EAL.save_loaded_asset(ia,False)
    key=unreal.Key();key.set_editor_property('key_name',key_name)
    mappings=[m for m in mappings if m.action != ia]
    mappings.append(unreal.EnhancedActionKeyMapping(action=ia,key=key))
    cdo.set_editor_property(prop,ia)
data.set_editor_property('mappings',mappings);imc.set_editor_property('default_key_mappings',data)
assert EAL.save_loaded_asset(imc,False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert EAL.save_loaded_asset(bp,False)
content=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))/'Training'
for name,_,_ in specs:assert (content/('IA_'+name+'.uasset')).is_file()
print('M3 assets saved; placeholder animation explicitly retained.')
