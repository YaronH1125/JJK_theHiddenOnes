# -*- coding: utf-8 -*-
import unreal
TAG = "[M2FixPC]"
def log(m): unreal.log(f"{TAG} {m}")
EAL = unreal.EditorAssetLibrary
ia_attack = unreal.load_object(None, "/Game/Training/IA_Attack.IA_Attack")
bp_pc = unreal.load_object(None, "/Game/Training/BP_ArenaPlayerController.BP_ArenaPlayerController")
log(f"ia={ia_attack} bp_pc={bp_pc}")
pc_cdo = unreal.get_default_object(bp_pc.generated_class())
pc_cdo.set_editor_property("attack_action", ia_attack)
EAL.save_loaded_asset(bp_pc, only_if_is_dirty=False)
check = pc_cdo.get_editor_property("attack_action")
log(f"PC attack_action = {check}")
# 顺带验证 DA 与 Fighter CDO
fd = unreal.load_object(None, "/Game/Training/DA_Fighter_Ishigori.DA_Fighter_Ishigori")
log(f"FighterDA attack_definition={fd.get_editor_property('attack_definition')} melee={fd.get_editor_property('melee_attack_ability')}")
bp_f = unreal.load_object(None, "/Game/Training/BP_Fighter.BP_Fighter")
f_cdo = unreal.get_default_object(bp_f.generated_class())
log(f"BP_Fighter CDO definition={f_cdo.get_editor_property('definition')} attack_da_def={f_cdo.get_editor_property('definition').get_editor_property('attack_definition') if f_cdo.get_editor_property('definition') else None}")
