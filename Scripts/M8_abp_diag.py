"""诊断石流龙趴地+变形：BP_Fighter 组件状态 / 骨架绑定 / 动画目标骨架 / Montage 引用。

只读，不写任何资产。用法：
    python Scripts/ue_python.py Scripts/M8_abp_diag.py
"""
import unreal

report = {}


def ppath(o):
    try:
        return o.get_path_name() if o else None
    except Exception:
        return repr(o)


def safe(fn, label):
    try:
        return fn()
    except Exception as e:
        return f'<ERR {label}: {e}>'


# ---- 1. BP_Fighter CDO --------------------------------------------------
bp = unreal.load_asset('/Game/Training/BP_Fighter')
report['bp'] = ppath(bp)
gen = unreal.load_object(None, '/Game/Training/BP_Fighter.BP_Fighter_C')
cdo = gen.get_default_object() if gen else None
if cdo:
    comp = safe(lambda: cdo.get_editor_property('mesh'), 'mesh-prop')
    report['mesh_comp_type'] = type(comp).__name__ if comp else None
    if comp and not isinstance(comp, str):
        report['mesh'] = ppath(safe(lambda: comp.get_editor_property('skeletal_mesh'), 'sm'))
        report['anim_class'] = ppath(safe(lambda: comp.get_editor_property('anim_class'), 'ac'))
        report['rel_loc'] = safe(lambda: [round(v, 2) for v in (
            comp.get_editor_property('relative_location').x,
            comp.get_editor_property('relative_location').y,
            comp.get_editor_property('relative_location').z)], 'loc')
        rr = safe(lambda: comp.get_editor_property('relative_rotation'), 'rot')
        if not isinstance(rr, str):
            report['rel_rot'] = {'pitch': round(rr.pitch, 2), 'yaw': round(rr.yaw, 2), 'roll': round(rr.roll, 2)}
        else:
            report['rel_rot'] = rr
        rs = safe(lambda: comp.get_editor_property('relative_scale3d'), 'scl')
        report['rel_scale'] = rs if isinstance(rs, str) else [round(v, 3) for v in (rs.x, rs.y, rs.z)]
        report['comp_anim_mode'] = safe(lambda: str(comp.get_editor_property('animation_mode')), 'am')

# ---- 2. SK_Ishigori -----------------------------------------------------
sk = unreal.load_asset('/Game/Characters/Ishigori/SK_Ishigori')
if sk:
    report['sk_skeleton'] = ppath(safe(lambda: sk.get_editor_property('skeleton'), 'sk-skel'))
    ib = safe(lambda: sk.get_editor_property('imported_bounds'), 'bounds')
    if not isinstance(ib, str):
        report['sk_bounds'] = {
            'origin': [round(v, 1) for v in (ib.origin.x, ib.origin.y, ib.origin.z)],
            'extent': [round(v, 1) for v in (ib.box_extent.x, ib.box_extent.y, ib.box_extent.z)],
        }
    report['sk_materials'] = safe(lambda: [ppath(m) for m in sk.get_editor_property('materials')][:2], 'mats')

# ---- 3. 骨架资产对照 ------------------------------------------------------
for name, p in [('SK_Mannequin(UE5)', '/Game/Characters/Mannequins/Meshes/SK_Mannequin'),
                ('tripo Skeleton', '/Game/Characters/Ishigori/tripo_convert_c9cda00f-349d-4bfd-900c-5909d0b0e001_Skeleton')]:
    a = unreal.load_asset(p)
    report[f'skel:{name}'] = ppath(a) if a else 'NOT FOUND'

# ---- 4. ABP_Unarmed 目标骨架 ---------------------------------------------
abp = unreal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed')
if abp:
    report['abp_unarmed_target_skeleton'] = safe(lambda: ppath(abp.get_editor_property('target_skeleton')), 'abp-skel')
    gen_abp = unreal.load_object(None, '/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C')
    if gen_abp:
        cdo2 = gen_abp.get_default_object()
        report['abp_unarmed_cdo_props'] = safe(lambda: [p for p in dir(cdo2) if not p.startswith('_')][:60], 'abp-cdo')

# ---- 5. 重定向动画的目标骨架（抽样） ---------------------------------------
ar = unreal.AssetRegistryHelpers.get_asset_registry()
retargeted = ar.get_assets_by_path('/Game/Characters/Ishigori/Retargeted')
report['retargeted_count'] = len(retargeted)
skel_of = {}
sample_names = []
for ad in retargeted:
    nm = str(ad.asset_name)
    sample_names.append(nm)
    if len(skel_of) < 6 or nm.endswith(('Idle', 'Jog_Fwd')):
        obj = unreal.load_asset(f"/Game/Characters/Ishigori/Retargeted/{nm}")
        if obj:
            s = safe(lambda: obj.get_editor_property('skeleton'), 'anim-skel')
            key = ppath(s) if not isinstance(s, str) else s
            skel_of.setdefault(key, []).append(nm)
report['retargeted_skeletons'] = {k: v[:3] for k, v in skel_of.items()}
report['retargeted_sample_names'] = sorted(sample_names)[:10]

# ---- 6. 数据资产里的 Montage 引用 -----------------------------------------
def dump_da(path, fields):
    da = unreal.load_asset(path)
    if not da:
        return 'NOT FOUND'
    out = {}
    for f in fields:
        v = safe(lambda: da.get_editor_property(f), f)
        if isinstance(v, str):
            out[f] = v
        else:
            try:
                out[f] = ppath(v.load_synchronous()) if v and v.asset_name else None
            except Exception:
                out[f] = repr(v)
    return out

MONT = ['montage', 'hit_react_montage', 'dodge_montage']
report['DA_Fighter_Ishigori'] = dump_da('/Game/Training/DA_Fighter_Ishigori',
                                        ['display_name'] + MONT)
for da_name in ['DA_M3_A1', 'DA_M3_HeavyPunch', 'DA_M3_Kick']:
    report[da_name] = dump_da(f'/Game/Training/{da_name}', MONT)

print('M8_ABP_DIAG_BEGIN')
print(unreal.SystemLibrary) if False else None
import json
print(json.dumps(report, ensure_ascii=False, indent=1, default=str))
print('M8_ABP_DIAG_END')
