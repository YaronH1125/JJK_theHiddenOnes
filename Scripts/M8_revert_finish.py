"""无 PIE 环境下完成 Montage 回滚 + 落盘校验 + 重启 PIE。

    python Scripts/ue_python.py Scripts/M8_revert_finish.py
"""
import json
import unreal

R = {'steps': []}
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
try:
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
        R['steps'].append('PIE ended')
except Exception:
    pass

REVERT_MAP = {
    'SK_Ishigori_MM_Attack_01': '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01',
    'SK_Ishigori_MM_HitReact_Front_Lgt_01': '/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01',
    'SK_Ishigori_MM_Dash': '/Game/Training/Movement/A_Dodge',
}


def revert_or_fix(mpath):
    m = unreal.load_asset(mpath)
    tracks = m.get_editor_property('slot_anim_tracks')
    swapped, rebuilt = [], False
    new_tracks = []
    for t in tracks:
        at = t.get_editor_property('anim_track')
        segs = at.get_editor_property('anim_segments')
        new_segs = []
        for s in segs:
            cur = s.get_editor_property('anim_reference')
            key = cur.get_name() if cur else ''
            if key in REVERT_MAP:
                s.set_editor_property('anim_reference', unreal.load_asset(REVERT_MAP[key]))
                swapped.append(key)
            elif cur is None:
                src = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01')
                s.set_editor_property('anim_reference', src)
                swapped.append('NULL->MM_Attack_01')
                rebuilt = True
            new_segs.append(s)
        at.set_editor_property('anim_segments', new_segs)
        t.set_editor_property('anim_track', at)
        new_tracks.append(t)
    m.set_editor_property('slot_anim_tracks', new_tracks)
    ok = unreal.EditorAssetLibrary.save_asset(mpath, only_if_is_dirty=False)
    return {'file': mpath.rsplit('/', 1)[-1], 'swapped': swapped, 'rebuilt_null': rebuilt, 'saved': ok}


R['montages'] = [
    revert_or_fix('/Game/Training/AM_M2_A1'),
    revert_or_fix('/Game/Training/AM_M2_HitReact'),
    revert_or_fix('/Game/Training/Movement/AM_Dodge'),
]

les.editor_request_begin_play()
R['steps'].append('PIE begin requested')

print('M8FIN_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8FIN_END')
