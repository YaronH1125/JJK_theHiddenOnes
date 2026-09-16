# -*- coding: utf-8 -*-
"""M2 资产创建：
- AM_M2_A1 / AM_M2_HitReact Montage（AnimMontageFactory + 手工动画段）
- DA_M2_Attack_A1（AttackDefinition：单段拳击 35 伤害，配置时间窗口）
- DA_Fighter_Ishigori 关联攻击定义
- IA_Attack + IMC_Training 左键映射
- BP_ArenaPlayerController 攻击输入绑定
执行: python Scripts/ue_python.py Scripts/M2_create_assets.py
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
TAG = "[M2Assets]"


def log(m):
    unreal.log(f"{TAG} {m}")


SK_MANNY = unreal.load_object(None, "/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin")


def build_montage(name, anim_path, skeleton):
    path = "/Game/Training/" + name
    if EAL.does_asset_exist(path):
        log(f"{name} 已存在，重建动画段")
        montage = EAL.load_asset(path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", skeleton)
        montage = AT.create_asset(name, "/Game/Training", unreal.AnimMontage, factory)
        log(f"创建 Montage {name}")

    anim = unreal.load_object(None, anim_path)
    anim_len = anim.get_editor_property("sequence_length")

    segment = unreal.AnimSegment()
    segment.set_editor_property("anim_reference", anim)
    segment.set_editor_property("anim_start_time", 0.0)
    segment.set_editor_property("anim_end_time", anim_len)

    track = unreal.AnimTrack()
    track.set_editor_property("anim_segments", [segment])

    slot = unreal.SlotAnimationTrack()
    slot.set_editor_property("slot_name", "DefaultSlot")
    slot.set_editor_property("anim_track", track)

    montage.set_editor_property("slot_anim_tracks", [slot])
    montage.set_editor_property("rate_scale", 1.0)
    EAL.save_asset(path, only_if_is_dirty=False)
    log(f"{name}: 引擎读取长度 {montage.get_editor_property('sequence_length')}（源 {anim_len}）")
    return montage


am_attack = build_montage("AM_M2_A1",
                          "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01",
                          SK_MANNY)
am_hit = build_montage("AM_M2_HitReact",
                       "/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01",
                       SK_MANNY)

# ---------- AttackDefinition 数据资产 ----------
da_path = "/Game/Training/DA_M2_Attack_A1"
if not EAL.does_asset_exist(da_path):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("DataAssetClass", unreal.AttackDefinition)
    da = AT.create_asset("DA_M2_Attack_A1", "/Game/Training", unreal.AttackDefinition, factory)
    log("创建 DA_M2_Attack_A1")
else:
    da = EAL.load_asset(da_path)

da.set_editor_property("display_name", unreal.Text("拳击 A1（M2 单段）"))
da.set_editor_property("attack_type_name", "Punch")
da.set_editor_property("segment_id", 0)
da.set_editor_property("damage", 35.0)
da.set_editor_property("montage", am_attack)
da.set_editor_property("hit_react_montage", am_hit)
da.set_editor_property("trace_socket", "hand_r")
da.set_editor_property("trace_radius", 20.0)
da.set_editor_property("hit_stun_duration", 0.5)
# Montage notifies 字段受保护无法由 Python 写入（记录为偏差）：
# 本阶段窗口使用配置时间；通知类已就绪，后续可在资产编辑器中手工添加并改回开关
da.set_editor_property("bWindowFromAnimNotifies", False)
da.set_editor_property("window_start_time", 0.25)
da.set_editor_property("window_end_time", 0.45)
EAL.save_asset(da_path, only_if_is_dirty=False)
log("DA_M2_Attack_A1 配置完成")

# ---------- 角色定义关联 ----------
fighter_da_path = "/Game/Training/DA_Fighter_Ishigori"
fighter_da = EAL.load_asset(fighter_da_path)
fighter_da.set_editor_property("attack_definition", da)
melee_default = fighter_da.get_editor_property("melee_attack_ability")
log(f"MeleeAttackAbility（CDO 默认）= {melee_default}")
if melee_default is None:
    melee_default = unreal.load_object(None, "/Script/JJK_theHiddenOnes.MeleeComboAbility")
    fighter_da.set_editor_property("melee_attack_ability", melee_default)
EAL.save_asset(fighter_da_path, only_if_is_dirty=False)
log("DA_Fighter_Ishigori 已关联攻击定义")

# ---------- 输入：使用同一套幂等创建、保存与落盘检查 ----------
from pathlib import Path
import runpy
runpy.run_path(str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
                   / 'Scripts/M2_fix_inputs.py'))
log("M2 资产创建完成")
