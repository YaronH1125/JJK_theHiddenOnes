"""M6: 为 DA_Fighter_Ishigori 配置三个新能力类（移动炮/超蓄力炮/领域展开）。

前置：编辑器已启动且 C++ 模块已编译（构建通过后重启编辑器）。
幂等：重复运行只覆盖同值。
"""
import unreal

unreal.log("[M6] setup start")

EAL = unreal.EditorAssetLibrary
folder = "/Game/Training/"
fighter = EAL.load_asset(folder + "DA_Fighter_Ishigori")
assert fighter is not None, "DA_Fighter_Ishigori 未找到"

classes = dict(
    mobile_blast_ability=unreal.MobileChargedBlastAbility.static_class(),
    stationary_blast_ability=unreal.StationaryChargedBlastAbility.static_class(),
    domain_expansion_ability=unreal.DomainExpansionAbility.static_class(),
)
for prop, cls in classes.items():
    assert cls is not None, f"{prop}: C++ 类未暴露给 Python"
    fighter.set_editor_property(prop, cls)

# 配置结构体使用 C++ 默认值（BlastConfig.h），此处仅回读校验
mobile = fighter.get_editor_property("mobile_blast")
domain = fighter.get_editor_property("domain_config")
assert abs(mobile.get_editor_property("cap_time") - 1.2) < 1e-3, "移动炮 CapTime 非 C++ 默认"
assert abs(domain.get_editor_property("duration") - 6.0) < 1e-3, "领域 Duration 非 C++ 默认"

assert EAL.save_loaded_asset(fighter, False)

# 回读校验
reloaded = EAL.load_asset(folder + "DA_Fighter_Ishigori")
assert str(reloaded.get_editor_property("mobile_blast_ability").get_name()) == "MobileChargedBlastAbility"
assert str(reloaded.get_editor_property("stationary_blast_ability").get_name()) == "StationaryChargedBlastAbility"
assert str(reloaded.get_editor_property("domain_expansion_ability").get_name()) == "DomainExpansionAbility"
unreal.log("[M6] fighter definition abilities configured and saved")
print("M6_SETUP_OK")
