# -*- coding: utf-8 -*-
"""修复 IMC_Training 中 action 为空的左键映射。"""
import unreal

TAG = "[M2FixIMC]"


def log(m):
    unreal.log(f"{TAG} {m}")


EAL = unreal.EditorAssetLibrary
imc = unreal.load_object(None, "/Game/Training/IMC_Training.IMC_Training")
ia_attack = unreal.load_object(None, "/Game/Training/IA_Attack.IA_Attack")

mapping_data = imc.get_editor_property("default_key_mappings")
mappings = list(mapping_data.get_editor_property("mappings"))

# 移除空 action 的映射与既有左键映射
cleaned = []
for m in mappings:
    action = m.get_editor_property("action")
    key_name = str(m.get_editor_property("key").get_editor_property("key_name"))
    if action is None:
        log(f"移除空 action 映射: key={key_name}")
        continue
    if key_name == "LeftMouseButton" and action.get_name() != "IA_Attack":
        log(f"移除冲突映射: {key_name} -> {action.get_name()}")
        continue
    cleaned.append(m)

key = unreal.Key()
key.set_editor_property("key_name", "LeftMouseButton")
cleaned.append(unreal.EnhancedActionKeyMapping(action=ia_attack, key=key))

mapping_data.set_editor_property("mappings", cleaned)
imc.set_editor_property("default_key_mappings", mapping_data)
EAL.save_loaded_asset(imc, only_if_is_dirty=False)

check = {str(m.get_editor_property("key").get_editor_property("key_name")): m.action.get_name()
         for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
         if m.get_editor_property("action")}
log(f"修复后映射: {check}")
