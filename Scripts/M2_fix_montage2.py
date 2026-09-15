# -*- coding: utf-8 -*-
"""打开 Montage 编辑器触发缓存重算，保存后关闭。"""
import unreal

TAG = "[M2MontageFix2]"


def log(m):
    unreal.log(f"{TAG} {m}")


EAL = unreal.EditorAssetLibrary

for name, expect in [("AM_M2_A1", 1.0), ("AM_M2_HitReact", 0.7)]:
    path = "/Game/Training/" + name
    opened = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([EAL.load_asset(path)])
    log(f"{name} 打开编辑器 -> {opened}")
    m = unreal.load_object(None, path + "." + name)
    log(f"{name} 打开后长度 = {m.get_editor_property('sequence_length')}")
    if abs(m.get_editor_property("sequence_length") - expect) < 0.05:
        EAL.save_asset(path, only_if_is_dirty=False)
        log(f"{name} 已保存")
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(EAL.load_asset(path))

for name, expect in [("AM_M2_A1", 1.0), ("AM_M2_HitReact", 0.7)]:
    m = unreal.load_object(None, "/Game/Training/" + name + "." + name)
    log(f"{name} 最终长度 = {m.get_editor_property('sequence_length')}（期望 {expect}）")
