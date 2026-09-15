# -*- coding: utf-8 -*-
"""尝试让 Montage 重算缓存长度。"""
import unreal

TAG = "[M2MontageFix]"


def log(m):
    unreal.log(f"{TAG} {m}")


EAL = unreal.EditorAssetLibrary

for name, expect in [("AM_M2_A1", 1.0), ("AM_M2_HitReact", 0.7)]:
    path = "/Game/Training/" + name
    m = unreal.load_object(None, path + "." + name)
    length = m.get_editor_property("sequence_length")
    if abs(length - expect) < 0.05:
        log(f"{name} 长度正常 {length}")
        continue

    # 方法 1：直接写 sequence_length（若可写）
    try:
        m.set_editor_property("sequence_length", expect)
        log(f"{name} 直接写长度成功 -> {m.get_editor_property('sequence_length')}")
    except Exception as e:
        log(f"{name} 直接写长度失败: {e}")

    # 方法 2：切换 rate_scale 触发 PostEditChange 重算
    try:
        rs = m.get_editor_property("rate_scale")
        m.set_editor_property("rate_scale", 0.99)
        m.set_editor_property("rate_scale", rs)
        log(f"{name} rate_scale 触发后长度 -> {m.get_editor_property('sequence_length')}")
    except Exception as e:
        log(f"{name} rate_scale 触发失败: {e}")

    EAL.save_asset(path, only_if_is_dirty=False)

# 重载再查
for name, expect in [("AM_M2_A1", 1.0), ("AM_M2_HitReact", 0.7)]:
    m = unreal.load_object(None, "/Game/Training/" + name + "." + name)
    log(f"{name} 最终长度 = {m.get_editor_property('sequence_length')}（期望 {expect}）")
