"""重建 3 个 Montage（工厂创建即脏，首存捕获轨道）：回滚到 Mannequin 原版动画。

    python Scripts/ue_python.py Scripts/M8_montage_recreate.py
"""
import json
import unreal

R = {'steps': []}

JOBS = [
    ('/Game/Training/AM_M2_A1',
     '/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01'),
    ('/Game/Training/AM_M2_HitReact',
     '/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01'),
    ('/Game/Training/Movement/AM_Dodge',
     '/Game/Training/Movement/A_Dodge'),
]

for mpath, seq_path in JOBS:
    pkg, name = mpath.rsplit('/', 1)
    entry = {'file': name}
    try:
        if unreal.EditorAssetLibrary.does_asset_exist(mpath):
            unreal.EditorAssetLibrary.delete_asset(mpath)
        sk = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
        fac = unreal.AnimMontageFactory()
        fac.set_editor_property('target_skeleton', sk)
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, pkg, unreal.AnimMontage, fac)
        entry['created'] = bool(m)
        seq = unreal.load_asset(seq_path)
        seg = unreal.AnimSegment()
        seg.set_editor_property('anim_reference', seq)
        for fld, val in [('start_pos', 0.0), ('anim_start_time', 0.0),
                         ('anim_end_time', seq.get_play_length()),
                         ('play_rate', 1.0), ('looping', False)]:
            try:
                seg.set_editor_property(fld, val)
            except Exception:
                entry.setdefault('readonly', []).append(fld)
        at = unreal.AnimTrack()
        at.set_editor_property('anim_segments', [seg])
        t = unreal.SlotAnimationTrack()
        t.set_editor_property('slot_name', 'DefaultSlot')
        t.set_editor_property('anim_track', at)
        m.set_editor_property('slot_anim_tracks', [t])
        ok = unreal.EditorAssetLibrary.save_asset(mpath, only_if_is_dirty=False)
        entry['saved'] = ok
        entry['len'] = round(m.get_play_length(), 2)
    except Exception as e:
        entry['err'] = str(e)[:200]
    R['steps'].append(entry)
    unreal.log(f'[M8_RECREATE] {entry}')

print('M8RC_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8RC_END')
