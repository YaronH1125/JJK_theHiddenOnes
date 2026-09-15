# -*- coding: utf-8 -*-
"""M1 资产创建：薄蓝图、角色定义、输入资产、颜色标识材质。
在编辑器控制台执行: py "F:/GameStudy/JJK_theHiddenOnes/Scripts/M1_create_assets.py"
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
TAG = "[M1Assets]"


def log(msg):
    unreal.log(f"{TAG} {msg}")


def log_warn(msg):
    unreal.log_warning(f"{TAG} {msg}")


# ---------- 0. 目录 ----------
for folder in ["Game/Training", "Game/Maps"]:
    if not EAL.does_directory_exist("/" + folder):
        EAL.make_directory("/" + folder)
        log(f"mkdir /{folder}")

# ---------- 1. 薄蓝图 ----------
def create_bp(parent_class, name):
    path = "/Game/Training/" + name
    if EAL.does_asset_exist(path):
        log(f"{name} 已存在，跳过创建")
        return EAL.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("ParentClass", parent_class)
    asset = AT.create_asset(name, "/Game/Training", unreal.Blueprint, factory)
    log(f"创建蓝图 {name}: {asset}")
    return asset


bp_fighter = create_bp(unreal.FighterCharacter, "BP_Fighter")
bp_gm = create_bp(unreal.TrainingGameMode, "BP_TrainingGameMode")
bp_pc = create_bp(unreal.ArenaPlayerController, "BP_ArenaPlayerController")

# ---------- 2. 角色定义数据资产 ----------
da_path = "/Game/Training/DA_Fighter_Ishigori"
if not EAL.does_asset_exist(da_path):
    da_factory = unreal.DataAssetFactory()
    da_factory.set_editor_property("DataAssetClass", unreal.FighterDefinition)
    da = AT.create_asset("DA_Fighter_Ishigori", "/Game/Training", unreal.FighterDefinition, da_factory)
    log(f"创建 DA_Fighter_Ishigori: {da}")
else:
    da = EAL.load_asset(da_path)
    log("DA_Fighter_Ishigori 已存在，跳过创建")

da.set_editor_property("display_name", unreal.Text("石流龙(占位)"))
da.set_editor_property("max_health", 1000.0)
da.set_editor_property("initial_health", 1000.0)
da.set_editor_property("max_action_resource", 100.0)
da.set_editor_property("initial_action_resource", 100.0)
da.set_editor_property("max_energy", 100.0)
da.set_editor_property("initial_energy", 0.0)
da.set_editor_property("marker_color", unreal.LinearColor(0.0, 0.45, 1.0, 1.0))
EAL.save_asset(da_path)
log("DA 属性已保存（默认参数，可在编辑器调整）")

# ---------- 3. 颜色标识材质（半透明叠加，参数名 Tint） ----------
mat_path = "/Game/Training/M_FighterTint"
if not EAL.does_asset_exist(mat_path):
    mat_factory = unreal.MaterialFactoryNew()
    mat = AT.create_asset("M_FighterTint", "/Game/Training", unreal.Material, mat_factory)
    mel = unreal.MaterialEditingLibrary
    try:
        mel.set_material_property(mat, unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialProperty.MP_BLEND_MODE)
    except Exception as e:
        log_warn(f"设置 BlendMode 失败（保留默认不透明）: {e}")
    try:
        mel.set_material_property(mat, 0.35, unreal.MaterialProperty.MP_OPACITY)
    except Exception as e:
        log_warn(f"设置 Opacity 失败: {e}")
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    node.set_editor_property("parameter_name", "Tint")
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    EAL.save_asset(mat_path)
    log(f"创建材质 {mat_path}")
else:
    log("M_FighterTint 已存在，跳过创建")
mat = EAL.load_asset(mat_path)

# ---------- 4. 输入动作（复制模板 Bool 按钮型 IA） ----------
ia_lock = "/Game/Training/IA_LockTarget"
ia_recenter = "/Game/Training/IA_RecenterCamera"
if not EAL.does_asset_exist(ia_lock):
    EAL.duplicate_asset("/Game/Input/Actions/IA_Jump", ia_lock)
    log("创建 IA_LockTarget")
if not EAL.does_asset_exist(ia_recenter):
    EAL.duplicate_asset("/Game/Input/Actions/IA_Jump", ia_recenter)
    log("创建 IA_RecenterCamera")

# ---------- 5. 输入映射上下文（复制模板 IMC 并追加映射） ----------
imc_path = "/Game/Training/IMC_Training"
if not EAL.does_asset_exist(imc_path):
    EAL.duplicate_asset("/Game/Input/IMC_Default", imc_path)
    log("创建 IMC_Training")
imc = EAL.load_asset(imc_path)

ia_lock_obj = EAL.load_asset(ia_lock)
ia_recenter_obj = EAL.load_asset(ia_recenter)


def make_key(name):
    key = unreal.Key()
    key.set_editor_property("key_name", name)
    return key


try:
    mapping_data = imc.get_editor_property("default_key_mappings")
    mappings = list(mapping_data.get_editor_property("mappings"))
    have = {m.get_editor_property("action").get_path_name() for m in mappings if m.get_editor_property("action")}
    for action_obj, key_name in [(ia_lock_obj, "MiddleMouseButton"), (ia_recenter_obj, "C")]:
        if action_obj.get_path_name() in have:
            log(f"映射已存在: {action_obj.get_name()}")
            continue
        m = unreal.EnhancedActionKeyMapping()
        m.set_editor_property("action", action_obj)
        m.set_editor_property("key", make_key(key_name))
        mappings.append(m)
        log(f"追加映射 {key_name} -> {action_obj.get_name()}")
    mouse_imc = EAL.load_asset('/Game/Input/IMC_MouseLook')
    for mouse_mapping in mouse_imc.get_editor_property('default_key_mappings').mappings:
        if mouse_mapping.action.get_path_name() not in have:
            mappings.append(mouse_mapping)
    mapping_data.set_editor_property("mappings", mappings)
    imc.set_editor_property("default_key_mappings", mapping_data)
    EAL.save_asset(imc_path)
except Exception as e:
    log_warn(f"IMC 映射写入失败: {e}")

# ---------- 6. BP CDO 属性配置 ----------
def cdo_of(bp):
    return unreal.get_default_object(bp.generated_class())


try:
    da.set_editor_property("marker_overlay_material", mat)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert EAL.save_loaded_asset(mat, only_if_is_dirty=False)
    EAL.save_asset(da_path)
    log("DA 覆盖材质已设置")

    cdo = cdo_of(bp_fighter)
    cdo.set_editor_property("definition", da)
    cdo.set_editor_property("default_mapping_context", imc)

    tpc_gen = unreal.load_object(None, "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C")
    tpc_cdo = unreal.get_default_object(tpc_gen)
    tpc_mesh = tpc_cdo.get_editor_property("mesh")
    my_mesh = cdo.get_editor_property("mesh")
    my_mesh.set_editor_property("skeletal_mesh", tpc_mesh.get_editor_property("skeletal_mesh"))
    my_mesh.set_editor_property("anim_class", tpc_mesh.get_editor_property("anim_class"))
    for prop in ["relative_location", "relative_rotation", "relative_scale3d"]:
        my_mesh.set_editor_property(prop, tpc_mesh.get_editor_property(prop))

    for src in ["jump_action", "move_action", "look_action", "mouse_look_action"]:
        try:
            cdo.set_editor_property(src, tpc_cdo.get_editor_property(src))
        except Exception as e:
            log_warn(f"复制输入属性 {src} 失败: {e}")

    cdo_gm = cdo_of(bp_gm)
    cdo_gm.set_editor_property("fighter_class", bp_fighter.generated_class())
    cdo_gm.set_editor_property("fighter_definition", da)
    cdo_gm.set_editor_property("player_controller_class", bp_pc.generated_class())
    cdo_gm.set_editor_property("default_pawn_class", None)

    cdo_pc = cdo_of(bp_pc)
    cdo_pc.set_editor_property("lock_target_action", ia_lock_obj)
    cdo_pc.set_editor_property("recenter_camera_action", ia_recenter_obj)

    for bp in [bp_fighter, bp_gm, bp_pc]:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert EAL.save_loaded_asset(bp, only_if_is_dirty=False)
    log("BP CDO 属性配置完成并保存")
except Exception as e:
    log_warn(f"BP 配置失败: {e}")

log("全部完成")
