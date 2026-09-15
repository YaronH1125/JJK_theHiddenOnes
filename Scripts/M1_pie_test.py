# -*- coding: utf-8 -*-
"""M1 PIE 运行时验证脚本。需在 PIE 运行时执行。
用法: py "F:/GameStudy/JJK_theHiddenOnes/Scripts/M1_pie_test.py" <步骤>
步骤: state | move | stats_read | lock | recenter | unlock | kill | modify_p1
"""
import sys

import unreal

TAG = "[M1Test]"


def log(m):
    unreal.log(f"{TAG} {m}")


def get_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def get_fighters(world):
    return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.FighterCharacter)


def read_health(attr_set):
    data = attr_set.get_editor_property("health")
    try:
        return data.get_editor_property("current_value")
    except Exception:
        return -1.0


def main():
    step = sys.argv[1] if len(sys.argv) > 1 else "state"
    world = get_world()
    if world is None:
        log("PIE 未运行")
        return

    fighters = get_fighters(world)
    log(f"战斗角色数量={len(fighters)}")
    if len(fighters) == 0:
        return

    pc = unreal.GameplayStatics.get_player_controller(world, 0)

    if step == "state":
        for f in fighters:
            attr = f.get_editor_property("attribute_set")
            log(f"{f.get_name()} 位置={f.get_actor_location()} 朝向={f.get_actor_rotation()} "
                f"Health={read_health(attr)} 初始化完成={f.is_stats_initialized()}")

    elif step == "move":
        f = unreal.GameplayStatics.get_player_pawn(world, 0)
        log(f"移动前位置={f.get_actor_location()}")
        state = {"n": 0}

        def tick(_dt):
            state["n"] += 1
            if state["n"] > 90:
                unreal.unregister_slate_post_tick_callback(state["handle"])
                log(f"输入注入结束，位置={f.get_actor_location()}")
                return
            f.do_move(0.0, 1.0)

        state["handle"] = unreal.register_slate_post_tick_callback(tick)

    elif step == "stats_read":
        for f in fighters:
            attr = f.get_editor_property("attribute_set")
            log(f"{f.get_name()} Health={read_health(attr)}")

    elif step == "modify_p1":
        # T07 隔离检查：只改玩家一方生命
        target = None
        for f in fighters:
            if str(f.get_editor_property("fighter_role")) == "Player":
                target = f
                break
        if target is None:
            log("未找到玩家角色")
            return
        attr = target.get_editor_property("attribute_set")
        h = attr.get_editor_property("health")
        h.set_editor_property("current_value", 500.0)
        h.set_editor_property("base_value", 500.0)
        attr.set_editor_property("health", h)
        for f in fighters:
            log(f"{f.get_name()} Health={read_health(f.get_editor_property('attribute_set'))}")

    elif step == "lock":
        pc.toggle_lock()

    elif step == "recenter":
        pc.recenter_camera_to_target()

    elif step == "unlock":
        pc.toggle_lock()

    elif step == "kill":
        pc.jjk_kill_target()

    elif step == "reinit":
        pc.jjk_reinit_fighters()

    else:
        log(f"未知步骤 {step}")


main()
