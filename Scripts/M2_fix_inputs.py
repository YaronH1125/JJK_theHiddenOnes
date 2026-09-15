# -*- coding: utf-8 -*-
"""重建 IA_Attack、修复 IMC 映射与控制器绑定。"""
import unreal

TAG = "[M2FixInput]"


def log(m):
    unreal.log(f"{TAG} {m}")


EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

ia_path = "/Game/Training/IA_Attack"
log(f"does_asset_exist: {EAL.does_asset_exist(ia_path)}")
log(f"does_directory_exist /Game/Training: {EAL.does_directory_exist('/Game/Training')}")

if not EAL.does_directory_exist("/Game/Training"):
    EAL.make_directory("/Game/Training")
if EAL.does_asset_exist(ia_path):
    EAL.delete_asset(ia_path)
    log("删除残留 IA_Attack")

dup = EAL.duplicate_asset("/Game/Input/Actions/IA_Jump", ia_path)
log(f"duplicate -> {dup}")
if dup is None:
    # 备选：AssetTools 创建
    ia_class = unreal.load_object(None, "/Script/EnhancedInput.InputAction")
    log(f"InputAction class = {ia_class}")
    dup = AT.create_asset("IA_Attack", "/Game/Training", ia_class, None)
    log(f"create_asset fallback -> {dup}")

ia_attack = unreal.load_object(None, "/Game/Training/IA_Attack.IA_Attack")
log(f"ia_attack loaded: {ia_attack}")
if ia_attack is None:
    raise RuntimeError("IA_Attack 创建失败")

EAL.save_asset(ia_path, only_if_is_dirty=False)

# IMC 映射（构造器模式）
imc = unreal.load_object(None, "/Game/Training/IMC_Training.IMC_Training")
mapping_data = imc.get_editor_property("default_key_mappings")
mappings = list(mapping_data.get_editor_property("mappings"))
cleaned = []
for m in mappings:
    action = m.get_editor_property("action")
    key_name = str(m.get_editor_property("key").get_editor_property("key_name"))
    if action is None or key_name == "LeftMouseButton":
        continue
    cleaned.append(m)
key = unreal.Key()
key.set_editor_property("key_name", "LeftMouseButton")
cleaned.append(unreal.EnhancedActionKeyMapping(action=ia_attack, key=key))
mapping_data.set_editor_property("mappings", cleaned)
imc.set_editor_property("default_key_mappings", mapping_data)
EAL.save_loaded_asset(imc, only_if_is_dirty=False)

final = {str(m.get_editor_property("key").get_editor_property("key_name")): m.action.get_name()
         for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
         if m.get_editor_property("action")}
log(f"最终映射: LeftMouseButton -> {final.get('LeftMouseButton')}")

# 控制器 CDO 绑定
bp_pc = EAL.load_asset("/Game/Training/BP_ArenaPlayerController")
pc_cdo = unreal.get_default_object(bp_pc.generated_class())
pc_cdo.set_editor_property("attack_action", ia_attack)
EAL.save_asset("/Game/Training/BP_ArenaPlayerController", only_if_is_dirty=False)
bound = pc_cdo.get_editor_property("attack_action")
log(f"PC attack_action = {bound}")
