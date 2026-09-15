# -*- coding: utf-8 -*-
"""闭环演示脚本：命中一次 -> 显示调试HUD -> FX 球。"""
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE not running"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
gm = pc.get_training_game_mode()
gm.call_method("JJKDebugHud")
p1 = gm.get_player_fighter()
p1.character_movement.stop_movement_immediately()
p1.set_actor_location(unreal.Vector(400, 0, 100), False, True)
unreal.SystemLibrary.execute_console_command(world, "JJK.DebugHitFX 1")
inp = p1.get_combat_input()
inp.notify_attack_pressed()
inp.notify_attack_released()
