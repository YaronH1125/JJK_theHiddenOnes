# -*- coding: utf-8 -*-
"""M1 关卡修复：出生标记关闭碰撞（避免挤开出生位置），保存关卡。
执行: py "F:/GameStudy/JJK_theHiddenOnes/Scripts/M1_fix_level.py"
"""
import unreal

TAG = "[M1Fix]"


def log(m):
    unreal.log(f"{TAG} {m}")


subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = subsys.get_all_level_actors()
fixed = 0
for a in actors:
    if a.get_class().get_name() == "PlayerStart":
        root = a.get_editor_property("root_component")
        root.set_collision_profile_name("NoCollision")
        fixed += 1
        log(f"{a.get_actor_label()} 碰撞已关闭")
log(f"完成，处理 {fixed} 个出生标记")
saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
log(f"save_dirty_packages -> {saved}")
