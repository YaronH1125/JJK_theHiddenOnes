# -*- coding: utf-8 -*-
"""M1 白盒擂台关卡：直径 24m 圆形场地 + 12 段围墙 + 出生标记 + 基础灯光。
执行: py "F:/GameStudy/JJK_theHiddenOnes/Scripts/M1_create_level.py"
参数依据 Docs/07_擂台与视觉参考.md：半径 1200cm，P1(-500,0) 朝 +X，P2(500,0) 朝 -X。
"""
import math

import unreal

EL = unreal.EditorLevelLibrary
EAL = unreal.EditorAssetLibrary
TAG = "[M1Level]"


def log(msg):
    unreal.log(f"{TAG} {msg}")


LEVEL_PATH = "/Game/Maps/L_TrainingArena"

# ---------- 1. 新建并加载关卡 ----------
if not EAL.does_asset_exist(LEVEL_PATH):
    ok = EL.new_level(LEVEL_PATH)
    log(f"new_level -> {ok}")
else:
    EL.load_level(LEVEL_PATH)
    log("关卡已存在，直接加载")

# 清空模板预设 Actor（保留 WorldSettings）
for actor in EL.get_all_level_actors():
    if actor.get_class().get_name() == "WorldSettings":
        continue
    EL.destroy_actor(actor)
log("清空预设 Actor 完成")

world = EL.get_editor_world()


def spawn_from_object(obj, loc, rot, scale, label):
    a = EL.spawn_actor_from_object(obj, unreal.Vector(*loc), unreal.Rotator(*rot))
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_actor_label(label)
    return a


def spawn_from_class(cls, loc, rot=(0.0, 0.0, 0.0), label=None):
    a = EL.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


cyl = unreal.load_object(None, "/Engine/BasicShapes/Cylinder.Cylinder")
cube = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")

# ---------- 2. 地面：半径 1200cm 圆柱，顶面 Z=0 ----------
floor = spawn_from_object(cyl, (0, 0, -50), (0, 0, 0), (12, 12, 1), "ArenaFloor")
log(f"地面: {floor.get_actor_label()} 位置={floor.get_actor_location()}")

# ---------- 3. 围墙：12 段立方体，环半径 640cm，高 200cm ----------
wall_len = 2.0 * math.pi * 640.0 / 12.0
for i in range(12):
    ang_deg = i * 30.0
    ang = math.radians(ang_deg)
    cx = 640.0 * math.cos(ang)
    cy = 640.0 * math.sin(ang)
    w = spawn_from_object(cube, (cx, cy, 100), (0, 0, ang_deg), (wall_len / 100.0, 0.5, 2.0), f"ArenaWall_{i:02d}")
log("12 段围墙生成完成")

# ---------- 4. 出生标记（视觉参考；实际出生由 GameMode 配置决定） ----------
spawn_from_class(unreal.PlayerStart, (-500, 0, 96), (0, 0, 0), "PlayerStart_P1")
spawn_from_class(unreal.PlayerStart, (500, 0, 96), (0, 0, 180), "PlayerStart_P2")

# ---------- 5. 基础灯光与天空 ----------
spawn_from_class(unreal.DirectionalLight, (0, 0, 500), (0, -45, 0), "SunLight")
sun = None
for a in EL.get_all_level_actors():
    if a.get_class().get_name() == "DirectionalLight":
        sun = a
        break
if sun is not None:
    comp = sun.get_editor_property("light_component")
    comp.set_editor_property("intensity", 3.0)
spawn_from_class(unreal.SkyLight, (0, 0, 300), (0, 0, 0), "SkyLight")
spawn_from_class(unreal.SkyAtmosphere, (0, 0, 0), (0, 0, 0), "SkyAtmosphere")
spawn_from_class(unreal.ExponentialHeightFog, (0, 0, 0), (0, 0, 0), "HeightFog")

# ---------- 6. World Settings 覆盖 GameMode ----------
ws = world.get_world_settings()
gm_class = unreal.load_object(None, "/Game/Training/BP_TrainingGameMode.BP_TrainingGameMode_C")
if gm_class is not None:
    ws.set_editor_property("default_game_mode", gm_class)
    log(f"WorldSettings GameMode -> {gm_class.get_name()}")
else:
    unreal.log_warning(f"{TAG} BP_TrainingGameMode 未找到，GameMode 覆盖跳过")

# ---------- 7. 保存 ----------
saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(save_map_packages=True, save_content_packages=False)
log(f"save_dirty_packages -> {saved}")
log("关卡创建完成")
