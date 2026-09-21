"""石流龙动画接入主脚本：
1. 保险：给用到的重定向动画补 skeleton 引用（当前为 null）
2. 三个 Montage 原地换轨到重定向动画（DA 引用不变）
3. 创建 BS_Ishigori_Idle_Walk_Run（克隆原 BS 采样布局）
4. 复制 ABP_Unarmed -> ABP_Ishigori（图结构完整保留，等人工换 5 个动画引用）
5. BP_Fighter.AnimClass 切到 ABP_Ishigori_C
6. 全部保存

    python Scripts/ue_python.py Scripts/M8_wire_ishigori.py
"""
import json
import unreal

RET = '/Game/Characters/Ishigori/Retargeted'
SKEL_PATH = '/Game/Characters/Mannequins/Meshes/SK_Mannequin'
R = {'steps': []}


def log(msg):
    R['steps'].append(msg)
    unreal.log(f'[M8_WIRE] {msg}')


def save(asset_path):
    ok = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    log(f'save {asset_path} -> {ok}')
    return ok


SKEL = unreal.load_asset(SKEL_PATH)

# ---- 1. skeleton 保险 ----------------------------------------------------
used = ['SK_Ishigori_MM_Idle', 'SK_Ishigori_MM_Jump', 'SK_Ishigori_MM_Land',
        'SK_Ishigori_MM_Fall_Loop', 'SK_Ishigori_MM_Attack_01',
        'SK_Ishigori_MM_HitReact_Front_Lgt_01', 'SK_Ishigori_MM_Dash']
for wj in ('Walk', 'Jog'):
    for d in ('Fwd', 'Bwd', 'Left', 'Right', 'Fwd_Left', 'Fwd_Right', 'Bwd_Left', 'Bwd_Right'):
        used.append(f'SK_Ishigori_MF_Unarmed_{wj}_{d}')
fixed_skel = 0
for name in used:
    p = f'{RET}/{name}'
    seq = unreal.load_asset(p)
    if not seq:
        log(f'MISSING retargeted anim: {p}')
        continue
    if seq.get_editor_property('skeleton') is None:
        try:
            seq.set_editor_property('skeleton', SKEL)
            fixed_skel += 1
        except Exception as e:
            log(f'skeleton set failed on {name}: {e}')
log(f'skeleton insurance: {fixed_skel}/{len(used)}')

# ---- 2. Montage 原地换轨 --------------------------------------------------
MONTAGE_MAP = {
    'MM_Attack_01': f'{RET}/SK_Ishigori_MM_Attack_01',
    'MM_HitReact_Front_Lgt_01': f'{RET}/SK_Ishigori_MM_HitReact_Front_Lgt_01',
    'MM_Dash': f'{RET}/SK_Ishigori_MM_Dash',
}


def swap_montage(mpath):
    m = unreal.load_asset(mpath)
    if not m:
        return f'{mpath}: NOT FOUND'
    tracks = m.get_editor_property('slot_anim_tracks')
    changed = []
    new_tracks = []
    for t in tracks:
        at = t.get_editor_property('anim_track')
        segs = at.get_editor_property('anim_segments')
        new_segs = []
        for s in segs:
            cur = s.get_editor_property('anim_reference')
            key = cur.get_name() if cur else ''
            if key in MONTAGE_MAP:
                s.set_editor_property('anim_reference', unreal.load_asset(MONTAGE_MAP[key]))
                changed.append(key)
            new_segs.append(s)
        at.set_editor_property('anim_segments', new_segs)
        t.set_editor_property('anim_track', at)
        new_tracks.append(t)
    m.set_editor_property('slot_anim_tracks', new_tracks)
    save(mpath)
    return f'{mpath}: swapped {changed}, len={m.get_play_length():.2f}s'


for mp in ['/Game/Training/AM_M2_A1', '/Game/Training/AM_M2_HitReact',
           '/Game/Training/Movement/AM_Dodge']:
    R.setdefault('montages', []).append(swap_montage(mp))

# ---- 3. BlendSpace -------------------------------------------------------
bs_src = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run')
bs_props = [p for p in dir(bs_src) if any(k in p for k in ('sampl', 'axis', 'parameter'))]
R['bs_props'] = bs_props

orig_samples = []
readable = True
try:
    for el in bs_src.get_editor_property('sample_data'):
        anim = el.get_editor_property('animation')
        pos = el.get_editor_property('position')
        orig_samples.append((anim.get_name() if anim else None,
                             (round(pos.x, 1), round(pos.y, 1))))
except Exception as e:
    readable = False
    R['bs_read_err'] = str(e)
R['bs_orig_samples'] = orig_samples if readable else None

RET_MAP = {n: f'{RET}/SK_Ishigori_{n}' for n, _ in orig_samples} if readable else {}
RET_MAP['MM_Idle'] = f'{RET}/SK_Ishigori_MM_Idle'

new_bs = None
if readable and orig_samples:
    try:
        facs = unreal.AssetToolsHelpers.get_asset_tools()
        bsf = unreal.BlendSpaceFactoryNew()
        bsf.set_editor_property('target_skeleton', SKEL)
        if unreal.EditorAssetLibrary.does_asset_exist('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run'):
            unreal.EditorAssetLibrary.delete_asset('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')
        new_bs = facs.create_asset('BS_Ishigori_Idle_Walk_Run', '/Game/Characters/Ishigori',
                                   unreal.BlendSpace, bsf)
        samples = []
        missing = []
        for name, pos in orig_samples:
            src = RET_MAP.get(name)
            anim = unreal.load_asset(src) if src else None
            if not anim:
                missing.append(name)
                continue
            smp = unreal.BlendSample()
            smp.set_editor_property('animation', anim)
            smp.set_editor_property('position', unreal.Vector(float(pos[0]), float(pos[1]), 0.0))
            samples.append(smp)
        new_bs.set_editor_property('sample_data', samples)
        R['bs_created'] = {'samples': len(samples), 'missing': missing}
        save('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')
    except Exception as e:
        R['bs_created'] = f'<ERR {e}>'
        new_bs = None

if not new_bs:
    # 兜底：复制原 BS（轴/采样位置保留），后续人工换 17 个采样动画
    if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run'):
        unreal.EditorAssetLibrary.duplicate_asset(
            '/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run',
            '/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')
    log('BS fallback: duplicated original; manual sample swap needed')
    save('/Game/Characters/Ishigori/BS_Ishigori_Idle_Walk_Run')

# ---- 4. ABP_Ishigori = 复制 ABP_Unarmed ----------------------------------
ABP_DST = '/Game/Characters/Ishigori/ABP_Ishigori'
if not unreal.EditorAssetLibrary.does_asset_exist(ABP_DST):
    ok = unreal.EditorAssetLibrary.duplicate_asset(
        '/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed', ABP_DST)
    log(f'ABP duplicate -> {ok}')
abp = unreal.load_asset(ABP_DST)
try:
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    log('ABP compiled')
except Exception as e:
    R['abp_compile'] = str(e)
save(ABP_DST)

# ---- 5. BP_Fighter.AnimClass + 状态记录 ----------------------------------
gen = unreal.load_object(None, '/Game/Training/BP_Fighter.BP_Fighter_C')
cdo = unreal.get_default_object(gen)
comp = None
try:
    comp = cdo.get_editor_property('mesh')
except Exception as e:
    R['cdo_mesh_err'] = str(e)
if comp is not None and not isinstance(comp, str):
    abp_c = unreal.load_object(None, '/Game/Characters/Ishigori/ABP_Ishigori.ABP_Ishigori_C')
    comp.set_editor_property('anim_class', abp_c)
    loc = comp.get_editor_property('relative_location')
    rot = comp.get_editor_property('relative_rotation')
    scl = comp.get_editor_property('relative_scale3d')
    R['fighter_mesh'] = {
        'sk': comp.get_editor_property('skeletal_mesh').get_path_name(),
        'anim_class': comp.get_editor_property('anim_class').get_path_name(),
        'loc': [round(loc.x, 2), round(loc.y, 2), round(loc.z, 2)],
        'rot': [round(rot.pitch, 2), round(rot.yaw, 2), round(rot.roll, 2)],
        'scale': [round(scl.x, 3), round(scl.y, 3), round(scl.z, 3)],
    }
    save('/Game/Training/BP_Fighter')
else:
    R['fighter_mesh'] = f'comp access failed: {comp!r}'

# ---- 6. 保存目录 ---------------------------------------------------------
n1 = unreal.EditorAssetLibrary.save_directory('/Game/Characters/Ishigori', only_if_is_dirty=True)
R['save_ishigori_dir'] = n1

print('M8_WIRE_BEGIN')
print(json.dumps(R, ensure_ascii=False, indent=1, default=str))
print('M8_WIRE_END')
