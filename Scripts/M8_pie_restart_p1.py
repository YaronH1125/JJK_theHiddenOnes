"""PIE 重启 + 就绪探测（阶段1：结束旧 PIE、启动新 PIE、注册就绪回调）。

    python Scripts/ue_python.py Scripts/M8_pie_restart_p1.py
"""
import sys
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
R = {}
try:
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
        R['ended'] = True
except Exception as e:
    R['end_err'] = str(e)

R['begin_requested'] = les.editor_request_begin_play()

# 就绪轮询：等 PIE 世界 + 玩家 pawn 出现
state = {'result': None, 'tries': 0}


def on_tick(dt):
    state['tries'] += 1
    if state['tries'] > 1200:  # ~20s 超时
        state['result'] = {'err': 'timeout waiting PIE'}
        unreal.unregister_slate_post_tick_callback(state['handle'])
        state['done'] = True
        return
    try:
        eds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        gw = eds.get_game_world()
        if not gw:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
        if not pawn:
            return
        mesh = pawn.get_editor_property('mesh')
        if not mesh:
            return
        origin = pawn.get_actor_location()
        ai = mesh.get_anim_instance()
        m = ai.get_current_active_montage() if ai else None
        state['result'] = {
            'world': gw.get_name(),
            'pawn': pawn.get_class().get_name(),
            'mesh': mesh.get_editor_property('skeletal_mesh').get_path_name(),
            'anim_class': ai.get_class().get_name() if ai else None,
            'active_montage': m.get_path_name() if m else None,
            'pawn_z': round(origin.z, 1),
            'head_z_rel': round(mesh.get_socket_location('head').z - origin.z, 1),
            'pelvis_z_rel': round(mesh.get_socket_location('pelvis').z - origin.z, 1),
            'scale': round(mesh.get_editor_property('relative_scale3d').x, 2),
        }
        unreal.unregister_slate_post_tick_callback(state['handle'])
        state['done'] = True
    except Exception as e:
        state.setdefault('errs', []).append(str(e))


state['handle'] = unreal.register_slate_post_tick_callback(on_tick)
sys.modules['_m8_pie_restart'] = state
print('M8_RESTART_P1_OK', R)
