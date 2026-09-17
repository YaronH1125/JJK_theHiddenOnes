"""M7-A01: 创建 IA_Aim（右鼠标）并接入 IMC_Training 与 BP_ArenaPlayerController。幂等。"""
import unreal

unreal.log("[A01] aim input setup start")
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
folder = "/Game/Training/"

path = folder + "IA_Aim"
ia = EAL.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset(
    "IA_Aim", folder.rstrip("/"), unreal.InputAction, unreal.InputAction_Factory())
ia.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
ia.set_editor_property("triggers", [])
assert EAL.save_loaded_asset(ia, False)

# IMC 映射：RightMouseButton → IA_Aim
imc = EAL.load_asset(folder + "IMC_Training")
data = imc.get_editor_property("default_key_mappings")
mappings = [m for m in data.get_editor_property("mappings") if m.action != ia]
key = unreal.Key(); key.set_editor_property("key_name", "RightMouseButton")
mappings.append(unreal.EnhancedActionKeyMapping(action=ia, key=key))
data.set_editor_property("mappings", mappings)
imc.set_editor_property("default_key_mappings", data)
assert EAL.save_loaded_asset(imc, False)

# BP_ArenaPlayerController CDO: aim_action = IA_Aim
bp = EAL.load_asset(folder + "BP_ArenaPlayerController")
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("aim_action", ia)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert EAL.save_loaded_asset(bp, False)

reloaded = EAL.load_asset(folder + "BP_ArenaPlayerController")
cdo2 = unreal.get_default_object(reloaded.generated_class())
assert str(cdo2.get_editor_property("aim_action").get_name()) == "IA_Aim"
unreal.log("[A01] aim input wired and saved")
print("A01_ASSETS_OK")
