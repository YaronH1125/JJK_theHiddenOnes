"""全量重烘焙 v2（带 source_mesh/target_mesh —— 之前静默失败的根因）。
批 A：23 个 MM/MF（游戏在用）；批 B：90 个 KB（战斗）。然后删除测试资产。

    python Scripts/ue_python.py Scripts/M8_batch_v2.py
"""
import json
import unreal

RET = '/Game/Characters/Ishigori/Retargeted'
R = {'steps': []}
ar = unreal.AssetRegistryHelpers.get_asset_registry()


def log(m):
    R['steps'].append(m)
    unreal.log(f'[M8_BATCH_V2] {m}')


def assetdata_for(path):
    obj = unreal.load_asset(path)
    if not obj:
        return None
    ad = ar.get_asset_by_object_path(obj.get_path_name())
    return ad if (ad and ad.is_valid()) else None


all_seqs = ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence'), True)
by_name = {}
for a in all_seqs:
    pkg = str(a.package_name)
    if 'Retargeted' in pkg:
        continue
    by_name.setdefault(str(a.asset_name), []).append(pkg)


def pick_source(name, prefer=('/Anims/Unarmed/',)):
    cands = by_name.get(name, [])
    if not cands:
        return None
    for pref in prefer:
        for c in cands:
            if pref in c:
                return c
    return sorted(cands)[0]


def run_batch(rt_path, src_mesh_path, ads, label):
    rt = unreal.load_asset(rt_path)
    src_mesh = unreal.load_asset(src_mesh_path)
    tgt_mesh = unreal.load_asset('/Game/Characters/Ishigori/SK_Ishigori')
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property('source_mesh', src_mesh)
    inputs.set_editor_property('target_mesh', tgt_mesh)
    inputs.set_editor_property('ik_retarget_asset', rt)
    inputs.set_editor_property('assets_to_retarget', ads)
    inputs.set_editor_property('target_path', RET)
    inputs.set_editor_property('prefix', 'SK_Ishigori_')
    inputs.set_editor_property('overwrite_existing_files', True)
    op = unreal.IKRetargetBatchOperation()
    res = op.run_batch_retarget(inputs)
    created = []
    for x in (res or []):
        try:
            if str(x.package_name).startswith(RET):
                created.append(str(x.asset_name))
        except Exception:
            pass
    log(f'{label}: wrote {len(created)}')
    return created


# ---- 批 A ---------------------------------------------------------------
NEEDED = ['MM_Idle', 'MM_Jump', 'MM_Land', 'MM_Fall_Loop', 'MM_Attack_01',
          'MM_HitReact_Front_Lgt_01', 'MM_Dash']
for wj in ('Walk', 'Jog'):
    for d in ('Fwd', 'Bwd', 'Left', 'Right', 'Fwd_Left', 'Fwd_Right', 'Bwd_Left', 'Bwd_Right'):
        NEEDED.append(f'MF_Unarmed_{wj}_{d}')

ads_a, missing = [], []
for n in NEEDED:
    p = pick_source(n)
    ad = assetdata_for(p) if p else None
    (ads_a.append(ad) if ad else missing.append(n))
R['missing_A'] = missing
if ads_a and not missing:
    out_a = run_batch('/Game/Characters/Mannequins/Meshes/RT_Quinn_to_Ishigori',
                      '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple', ads_a, 'batchA')
    R['out_A_count'] = len(out_a)
    R['out_A_names'] = out_a[:5]

# ---- 批 B ---------------------------------------------------------------
kb_names = []
for a in ar.get_assets_by_path(RET):
    nm = str(a.asset_name)
    if nm.startswith('SK_Ishigori_KB_'):
        kb_names.append(nm[len('SK_Ishigori_'):])
ads_b, missing_b = [], []
for n in sorted(set(kb_names)):
    ad = assetdata_for(f'/Game/FightingAnimsetPro/Animations/{n}')
    if not ad:
        p = pick_source(n)
        ad = assetdata_for(p) if p else None
    if ad:
        ads_b.append(ad)
    else:
        missing_b.append(n)
R['missing_B'] = missing_b
if ads_b:
    out_b = run_batch('/Game/Characters/Ishigori/RT_FASP_to_Ishigori',
                      '/Game/FightingAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin', ads_b, 'batchB')
    R['out_B_count'] = len(out_b)

# ---- 清理测试资产 -------------------------------------------------------
for p in ['/Game/Characters/Ishigori/Retargeted/SK_IGTEST3_MM_Jump']:
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        unreal.EditorAssetLibrary.delete_asset(p)
log('test asset deleted')

print('M8BV2_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8BV2_END')
