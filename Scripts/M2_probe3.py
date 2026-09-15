# -*- coding: utf-8 -*-
"""探测 Montage 动画段结构体暴露情况。"""
import unreal

TAG = "[M2Probe3]"


def log(m):
    unreal.log(f"{TAG} {m}")


am = unreal.load_object(None, "/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack")

for prop in ["composite_sections", "slot_anim_tracks", "anim_track", "sequence_length", "rate_scale"]:
    try:
        v = am.get_editor_property(prop)
        log(f"{prop} = {type(v).__name__} {v if not isinstance(v, (list,)) else len(v)}")
    except Exception as e:
        log(f"{prop} read failed: {e}")

try:
    tracks = am.get_editor_property("slot_anim_tracks")
    if tracks:
        t = tracks[0]
        log(f"slot={t.get_editor_property('slot_name')}")
        track = t.get_editor_property("anim_track")
        segs = track.get_editor_property("anim_segments")
        log(f"segments={len(segs)}")
        if segs:
            s = segs[0]
            for p in ["anim_reference", "start_pos", "anim_start_time", "anim_end_time", "length"]:
                try:
                    log(f"  segment.{p} = {s.get_editor_property(p)}")
                except Exception as e:
                    log(f"  segment.{p} read failed: {e}")
except Exception as e:
    log(f"slot track read failed: {e}")

# 新建 montage 后能否直接设置这些字段（用临时 montage 试）
factory = unreal.AnimMontageFactory()
log(f"factory target skeleton support: {factory.get_editor_property('target_skeleton') is None}")
