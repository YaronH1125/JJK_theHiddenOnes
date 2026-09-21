"""回退到 Quinn 占位：BP_Fighter(mesh/AnimClass/变换) + 3 个 Montage 轨道回滚。
石流龙资产全部保留不动（修好重定向后可一键切回）。

    python Scripts/ue_python.py Scripts/M8_revert_quinn.py
"""
import json
import sys
import unreal

R = {'steps': []}


def log(m):
    R['steps'].append(m)
    unreal.log(f'[M8_REVERT] {m}')


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
try:
    if les.is_in_play_in_editor():
        les.editor_request_end_play()
        log('PIE ended')
except Exception as e:
    log(f'pie end err: {e}')

# ---- 1. BP_Fighter → Quinn + ABP_Unarmed --------------------------------
gen = unreal.load_object(None, '/Game/Training/BP_Fighter.BP_Fighter_C')
cdo = unreal.get_default_object(gen)
comp = cdo.get_editor_property('mesh')
quinn = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
abp_unarmed = unreal.load_object(None,
    '/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C')
comp.set_editor_property('skeletal_mesh', quinn)
comp.set_editor_property('anim_class', abp_unarmed)
loc = comp.get_editor_property('relative_location')
loc.z = -89.0
comp.set_editor_property('relative_location', loc)
rot = comp.get_editor_property('relative_rotation')
rot.set_editor_property('pitch', 0.0)
rot.set_editor_property('yaw', -90.0)
rot.set_editor_property('roll', 0.0)
comp.set_editor_property('relative_rotation', rot)
comp.set_editor_property('relative_scale3d', unreal.Vector(1.0, 1.0, 1.0))
unreal.EditorAssetLibrary.save_asset('/Game/Training/BP_Fighter')
log('BP_Fighter -> Quinn + ABP_Unarmed (z=-89, yaw=-90, scale 1.0)')

# ---- 2. Montage 轨道回滚 --------------------------------------------------
REVERT_MAP = {
    'SK_Ishigori_MM_Attack_01': '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01',
    'SK_Ishigori_MM_HitReact_Front_Lgt_01': '/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01',
    'SK_Ishigori_MM_Dash': '/Game/Training/Movement/A_Dodge',
}


def revert_montage(mpath):
    m = unreal.load_asset(mpath)
    tracks = m.get_editor_property('slot_anim_tracks')
    new_tracks, swapped = [], []
    for t in tracks:
        at = t.get_editor_property('anim_track')
        segs = at.get_editor_property('anim_segments')
        new_segs = []
        for s in segs:
            cur = s.get_editor_property('anim_reference')
            key = cur.get_name() if cur else ''
            if key in REVERT_MAP:
                tgt = unreal.load_asset(REVERT_MAP[key])
                s.set_editor_property('anim_reference', tgt)
                swapped.append(key)
            new_segs.append(s)
        at.set_editor_property('anim_segments', new_segs)
        t.set_editor_property('anim_track', at)
        new_tracks.append(t)
    m.set_editor_property('slot_anim_tracks', new_tracks)
    ok = unreal.EditorAssetLibrary.save_asset(mpath)
    return {'file': mpath.rsplit('/', 1)[-1], 'swapped': swapped, 'saved': ok}


R['montages'] = [
    revert_montage('/Game/Training/AM_M2_A1'),
    revert_montage('/Game/Training/AM_M2_HitReact'),
    revert_montage('/Game/Training/Movement/AM_Dodge'),
]

# ---- 3. 重启 PIE --------------------------------------------------------
les.editor_request_begin_play()
log('PIE begin requested')

print('M8RV_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8RV_END')
