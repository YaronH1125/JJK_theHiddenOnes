"""PIE 探针：读玩家角色 mesh 的动画实例/骨骼世界位置/活动 Montage。

    python Scripts/ue_python.py Scripts/M8_pie_probe.py
"""
import json
import unreal

R = {}

eds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
R['has_game_world'] = False
try:
    gw = eds.get_game_world()
    R['has_game_world'] = gw is not None
except Exception as e:
    R['gw_err'] = str(e)
    gw = None

if gw:
    try:
        R['world_name'] = gw.get_name()
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        R['pawn'] = pawn.get_class().get_name() if pawn else None
        if pawn:
            mesh = pawn.get_editor_property('mesh')
            R['mesh'] = mesh.get_editor_property('skeletal_mesh').get_path_name() if mesh else None
            ai = mesh.get_anim_instance() if mesh else None
            R['anim_class'] = ai.get_class().get_name() if ai else None
            if ai and hasattr(ai, 'get_current_active_montage'):
                m = ai.get_current_active_montage()
                R['active_montage'] = m.get_path_name() if m else None
            origin = pawn.get_actor_location()
            R['pawn_z'] = round(origin.z, 1)
            if mesh:
                for bone in ('head', 'pelvis'):
                    try:
                        loc = mesh.get_socket_location(bone)
                        R[f'{bone}_z_rel'] = round(loc.z - origin.z, 1)
                    except Exception as e:
                        R[f'{bone}_err'] = str(e)
                R['mesh_scale'] = [round(v, 2) for v in (
                    mesh.get_editor_property('relative_scale3d').x,
                    mesh.get_editor_property('relative_scale3d').y,
                    mesh.get_editor_property('relative_scale3d').z)]
    except Exception as e:
        R['probe_err'] = str(e)
else:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    R['les_play_methods'] = [m for m in dir(les) if 'play' in m.lower() or 'simulat' in m.lower()]

print('M8_PIE_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8_PIE_END')
