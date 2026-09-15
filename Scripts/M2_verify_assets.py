# -*- coding: utf-8 -*-
"""M2 资产验证：重载后检查 Montage 段/长度、DA 配置、IMC 映射。"""
import unreal

TAG = "[M2Verify]"


def log(m):
    unreal.log(f"{TAG} {m}")


EAL = unreal.EditorAssetLibrary

for name in ["AM_M2_A1", "AM_M2_HitReact"]:
    path = "/Game/Training/" + name
    EAL.load_asset(path)  # 确保已加载
    # 强制重新加载以重算缓存长度
    m = unreal.load_object(None, path + "." + name)
    length = m.get_editor_property("sequence_length")
    tracks = m.get_editor_property("slot_anim_tracks")
    seg_info = []
    for t in tracks:
        for s in t.get_editor_property("anim_track").get_editor_property("anim_segments"):
            ref = s.get_editor_property("anim_reference")
            seg_info.append(f"{ref.get_name()} [{s.get_editor_property('anim_start_time'):.2f}-{s.get_editor_property('anim_end_time'):.2f}]")
    log(f"{name}: length={length} segments={seg_info}")

da = unreal.load_object(None, "/Game/Training/DA_M2_Attack_A1.DA_M2_Attack_A1")
log(f"DA: damage={da.get_editor_property('damage')} montage={da.get_editor_property('montage')} "
    f"window_notify={da.get_editor_property('bWindowFromAnimNotifies')} "
    f"window={da.get_editor_property('window_start_time')}-{da.get_editor_property('window_end_time')} "
    f"socket={da.get_editor_property('trace_socket')} stun={da.get_editor_property('hit_stun_duration')}")

fd = unreal.load_object(None, "/Game/Training/DA_Fighter_Ishigori.DA_Fighter_Ishigori")
log(f"FighterDA: attack_definition={fd.get_editor_property('attack_definition')} "
    f"melee={fd.get_editor_property('melee_attack_ability')}")

imc = unreal.load_object(None, "/Game/Training/IMC_Training.IMC_Training")
keys = {str(m.key.get_editor_property("key_name")): m.action.get_name()
        for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")}
log(f"IMC keys: {keys}")
