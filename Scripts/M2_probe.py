# -*- coding: utf-8 -*-
"""M2 前置探测：Montage 创建/通知 API、能力查询 API、现有资产状态。"""
import unreal

TAG = "[M2Probe]"


def log(m):
    unreal.log(f"{TAG} {m}")


# 1. Montage factory
log(f"has MontageFactory: {hasattr(unreal, 'MontageFactory')}")
log(f"has AnimMontageFactory: {hasattr(unreal, 'AnimMontageFactory')}")

# 2. Montage notify editing APIs
for name in dir(unreal.AnimMontage):
    if "notify" in name.lower() or "segment" in name.lower():
        log(f"AnimMontage.{name}")

# 3. AnimSequence notify APIs
for name in dir(unreal.AnimSequenceBase):
    if "notify" in name.lower():
        log(f"AnimSequenceBase.{name}")

# 4. Animation library helpers
for name in dir(unreal):
    if "AnimationLibrary" in name or "AnimationModifier" in name:
        log(f"unreal.{name}")

# 5. Check MM_Attack_01 skeleton + duration
try:
    atk = unreal.load_object(None, "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01")
    log(f"MM_Attack_01: skeleton={atk.get_editor_property('skeleton').get_name()} dur={atk.get_editor_property('sequence_length')}")
except Exception as e:
    log(f"MM_Attack_01 load failed: {e}")

# 6. Hit react anim
try:
    hr = unreal.load_object(None, "/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01")
    log(f"HitReact: skeleton={hr.get_editor_property('skeleton').get_name()} dur={hr.get_editor_property('sequence_length')}")
except Exception as e:
    log(f"HitReact load failed: {e}")

# 7. AbilitySystemInspector toolset for ability queries
insp = unreal.get_default_object(unreal.AbilitySystemInspectorToolset)
log(f"Inspector methods: {[m for m in dir(insp) if not m.startswith('_')][:20]}")

# 8. Existing GE-related python surface
log(f"has GameplayEffect: {hasattr(unreal, 'GameplayEffect')}")
log(f"has GameplayAbility: {hasattr(unreal, 'GameplayAbility')}")
