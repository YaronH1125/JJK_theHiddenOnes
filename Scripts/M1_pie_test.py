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


def read_health(fighter):
    values = unreal.get_default_object(unreal.AbilitySystemInspectorToolset).call_method('GetAttributeValues', (fighter,))
    return next(v.current_value for v in values if v.attribute_name == 'Health')


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
                f"Health={read_health(f)} 初始化次数={f.get_stats_init_count()}")

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
            log(f"{f.get_name()} Health={read_health(f)}")

    elif step == "modify_p1":
        # T07 隔离检查：使用临时角色配置走集中重置，不改共享 DA。
        target = pc.get_player_fighter()
        if target is None:
            log("未找到玩家角色")
            return
        original = target.definition
        debug_definition = unreal.FighterDefinition()
        debug_definition.set_editor_property('initial_health', 500)
        target.set_editor_property('definition', debug_definition)
        target.reset_to_initial_state()
        target.set_editor_property('definition', original)
        for f in fighters:
            log(f"{f.get_name()} Health={read_health(f)}")

    elif step == "lock":
        pc.toggle_lock()

    elif step == "recenter":
        pc.recenter_camera_to_target()

    elif step == "unlock":
        pc.toggle_lock()

    elif step == "kill":
        pc.call_method('JJKKillTarget')

    elif step == "reinit":
        pc.call_method('JJKReinitFighters')

    elif step == 'respawn':
        pc.call_method('JJKRespawnFighters')

    elif step == 'reset':
        pc.call_method('JJKResetFighters')

    else:
        log(f"未知步骤 {step}")


main()
