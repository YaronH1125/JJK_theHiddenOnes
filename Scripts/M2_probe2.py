# -*- coding: utf-8 -*-
"""探测 Montage 通知编辑 API。"""
import unreal

TAG = "[M2Probe2]"


def log(m):
    unreal.log(f"{TAG} {m}")


log(f"has new_object: {hasattr(unreal, 'new_object')}")
log(f"has AnimNotifyEvent: {hasattr(unreal, 'AnimNotifyEvent')}")

if hasattr(unreal, "AnimNotifyEvent"):
    props = [p for p in dir(unreal.AnimNotifyEvent) if not p.startswith("_")]
    log(f"AnimNotifyEvent props: {props}")

# 现有 Montage 的 notifies 属性（用模板 AM_ComboAttack 验证读取）
am = unreal.load_object(None, "/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack")
try:
    notifies = am.get_editor_property("notifies")
    log(f"AM_ComboAttack notifies count={len(notifies)}")
    for n in notifies[:5]:
        log(f"  notify entry: {n.get_editor_property('notify')} at {n.get_editor_property('start_time')}")
except Exception as e:
    log(f"notifies read failed: {e}")

# UAnimNotify_AttackWindowOpen 类是否已加载（本模块 C++ 类）
log(f"has AttackWindowOpen class: {unreal.load_object(None, '/Script/JJK_theHiddenOnes.AnimNotify_AttackWindowOpen') is not None}")
log(f"has MeleeComboAbility class: {unreal.load_object(None, '/Script/JJK_theHiddenOnes.MeleeComboAbility') is not None}")
log(f"has AttackDefinition class: {unreal.load_object(None, '/Script/JJK_theHiddenOnes.AttackDefinition') is not None}")
log(f"has DamageGameplayEffect class: {unreal.load_object(None, '/Script/JJK_theHiddenOnes.DamageGameplayEffect') is not None}")
