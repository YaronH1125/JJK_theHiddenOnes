"""安全收尾（v2，避免上版 import_text 崩溃）：
1) AM_Dodge 换轨 A_Dodge -> SK_Ishigori_MM_Dash
2) BS_Ishigori 采样：在原结构体副本上只替换 animation（position 保留在副本内）

    python Scripts/ue_python.py Scripts/M8_v2_safe.py
"""
import json
import unreal

RET = '/Game/Characters/Ishigori/Retargeted'
R = {'steps': []}

# ---- 1. AM_Dodge --------------------------------------------------------
m = unreal.load_asset('/Game/Training/Movement/AM_Dodge')
tracks = m.get_editor_property('slot_anim_tracks')
new_tracks, swapped = [], []
for t in tracks:
    at = t.get_editor_property('anim_track')
    segs = at.get_editor_property('anim_segments')
    new_segs = []
    for s in segs:
        cur = s.get_editor_property('anim_reference')
        if cur and cur.get_name() == 'A_Dodge':
            s.set_editor_property('anim_reference', unreal.load_asset(f'{RET}/SK_Ishigori_MM_Dash'))
            swapped.append('A_Dodge->SK_Ishigori_MM_Dash')
        new_segs.append(s)
    at.set_editor_property('anim_segments', new_segs)
    t.set_editor_property('anim_track', at)
    new_tracks.append(t)
m.set_editor_property('slot_anim_tracks', new_tracks)
ok = unreal.EditorAssetLibrary.save_asset('/Game/Training/Movement/AM_Dodge')
R['dodge'] = {'swapped': swapped, 'saved': ok}
unreal.log(f'[M8_V2] dodge swapped={swapped} saved={ok}')

# ---- 2. BS 采样（副本只换 animation） -----------------------------------
src = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run')
new_bs = unreal.load_asset('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')
samples, missing, kept = [], [], 0
for el in src.get_editor_property('sample_data'):
    anim = el.get_editor_property('animation')
    nm = anim.get_name() if anim else ''
    tgt_name = f'SK_Ishigori_MM_Idle' if nm == 'MM_Idle' else f'SK_Ishigori_{nm}'
    tgt = unreal.load_asset(f'{RET}/{tgt_name}')
    if not tgt:
        missing.append(nm)
        samples.append(el)
        continue
    try:
        el.set_editor_property('animation', tgt)
        kept += 1
    except Exception as e:
        R.setdefault('set_err', []).append(f'{nm}: {e}')
    samples.append(el)
R['bs'] = {'total': len(samples), 'swapped': kept, 'missing': missing}
if kept and not missing:
    new_bs.set_editor_property('sample_data', samples)
    ok = unreal.EditorAssetLibrary.save_asset('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')
    R['bs']['saved'] = ok
else:
    R['bs']['saved'] = False

print('M8_V2_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8_V2_END')
