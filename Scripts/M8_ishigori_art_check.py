"""M8 石流龙 AI 素材接入前验证：FBX -> UE 隔离试导入 -> 骨骼/身高/蒙皮/贴图核对。

只读源文件，只写 /Game/Training/ArtReview/，不碰 BP_Fighter / DA_Fighter_Ishigori /
任何现有动画或地图。目的：在真正接入之前回答三个问题——
  1. 骨骼命名与 SK_Mannequin 差多少（决定要不要重定向）
  2. 身高是否落在 190cm 基准
  3. 蒙皮/材质/贴图是否完整

用法（编辑器开着、且已启用 Python 远程执行）：
    python Scripts/ue_python.py Scripts/M8_ishigori_art_check.py
结果写到 Docs/开发过程/验收记录/M8_ArtCheck.json
"""
import hashlib
import json
from pathlib import Path
import re
import unreal

# ---------------------------------------------------------------- 配置区
# 导出的 FBX 目录：脚本会自动找里面最新的 .fbx
SOURCE_DIR = Path('F:/GameStudy/JJK_theHiddenOnes/AssetArchives/Ishigori_AI/03_rigged')
# 隔离试导入目录（可重建、已 gitignore，和 M1 的 ArtReview 同一个目录）
REVIEW_FOLDER = '/Game/Training/ArtReview'
MESH_NAME = 'SK_Ishigori_AI_Check'
# 基准身高：旧模型实测 190.479cm，工程以 190 为准
TARGET_HEIGHT_CM = 190.0
TOLERANCE_CM = 5.0
# UE5 Manny 核心骨骼（用于算命名差异；只列关键骨，不含手指细节）
MANNY_CORE = [
    'root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05',
    'neck_01', 'head', 'clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l',
    'clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r',
    'thigh_l', 'calf_l', 'foot_l', 'ball_l',
    'thigh_r', 'calf_r', 'foot_r', 'ball_r',
]
# 常见第三方人形命名 -> Manny（用于给出"如果不想重定向，可以改名"的对照）
COMMON_ALIASES = {
    'hips': 'pelvis', 'spine': 'spine_01', 'spine1': 'spine_02', 'chest': 'spine_03',
    'neck': 'neck_01', 'leftshoulder': 'clavicle_l', 'leftarm': 'upperarm_l',
    'leftforearm': 'lowerarm_l', 'lefthand': 'hand_l',
    'rightshoulder': 'clavicle_r', 'rightarm': 'upperarm_r',
    'rightforearm': 'lowerarm_r', 'righthand': 'hand_r',
    'leftupleg': 'thigh_l', 'leftleg': 'calf_l', 'leftfoot': 'foot_l', 'lefttoebase': 'ball_l',
    'rightupleg': 'thigh_r', 'rightleg': 'calf_r', 'rightfoot': 'foot_r', 'righttoebase': 'ball_r',
}
TEXTURE_EXT = {'.png', '.jpg', '.jpeg', '.tga', '.tif', '.tiff', '.exr', '.bmp', '.webp'}
# ---------------------------------------------------------------------

report = {
    'config': {'source_dir': str(SOURCE_DIR), 'target_height_cm': TARGET_HEIGHT_CM},
    'engine': unreal.SystemLibrary.get_engine_version(),
    'source_files': [],
    'textures_found': [],
    'mesh': None,
    'skeleton_comparison': None,
    'verdict': [],
    'blocking': [],
}


def fail(msg):
    report['blocking'].append(msg)
    print('M8_ART_CHECK_BLOCKED', msg)


# ---------------------------------------------------------- 1. 源文件盘点
if not SOURCE_DIR.exists():
    fail(f'源目录不存在：{SOURCE_DIR}。把 Tripo 导出的 FBX 和贴图放进这个目录再跑。')
    raise SystemExit(0)

for p in sorted(SOURCE_DIR.rglob('*')):
    if not p.is_file():
        continue
    entry = {'name': str(p.relative_to(SOURCE_DIR)), 'bytes': p.stat().st_size}
    if p.suffix.lower() == '.fbx':
        entry['sha256'] = hashlib.sha256(p.read_bytes()).hexdigest()
        entry['mtime'] = p.stat().st_mtime
    report['source_files'].append(entry)
    if p.suffix.lower() in TEXTURE_EXT:
        report['textures_found'].append(entry['name'])

fbx_files = [e for e in report['source_files'] if e['name'].lower().endswith('.fbx')]
if not fbx_files:
    fail(f'{SOURCE_DIR} 里没有 .fbx。先导出 FBX 再跑这个脚本。')
    raise SystemExit(0)

# 多个 FBX 时取最大的那个（带骨骼的通常最大）
main = max(fbx_files, key=lambda e: e['bytes'])
fbx_path = SOURCE_DIR / main['name']
report['chosen_fbx'] = main['name']

if not report['textures_found']:
    report['verdict'].append('未在源目录发现贴图文件——确认贴图已一并导出；若 FBX 内嵌贴图可忽略。')

# ------------------------------------------------- 2. 隔离试导入（不改现有资产）
eal = unreal.EditorAssetLibrary
full_path = f'{REVIEW_FOLDER}/{MESH_NAME}'


def import_mesh():
    if eal.does_asset_exist(full_path):
        eal.delete_asset(full_path)
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_materials = True
    options.import_textures = True
    options.import_as_skeletal = True
    options.create_physics_asset = False
    options.import_mesh = True
    options.import_animations = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    # 本次故意不指定 skeleton：要新建一套，才能读出 Tripo 自己的骨骼命名做比对
    task = unreal.AssetImportTask()
    task.filename = str(fbx_path)
    task.destination_path = REVIEW_FOLDER
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = [eal.load_asset(p) for p in task.imported_object_paths]
    mesh = next((a for a in assets if isinstance(a, unreal.SkeletalMesh)), None)
    if mesh is None:
        fail(f'导入未产出 SkeletalMesh。导入结果：{task.imported_object_paths}')
    return mesh, assets


mesh, imported = import_mesh()
if mesh is None:
    report['mesh'] = {'error': 'no skeletal mesh'}
else:
    skeleton = mesh.get_editor_property('skeleton')
    pose = skeleton.get_reference_pose()
    bones = [str(n) for n in pose.get_bone_names()]
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2

    report['mesh'] = {
        'asset': mesh.get_path_name(),
        'skeleton': skeleton.get_path_name() if skeleton else None,
        'bone_count': len(bones),
        'bones': bones,
        'height_cm': round(height, 3),
        'material_slots': [str(m.get_name()) if m else None for m in mesh.materials],
        'imported_assets': [a.get_path_name() for a in imported if a],
    }

    # ------------------------------------------------- 3. 与 Manny 比对
    lower = {b.lower(): b for b in bones}
    matched, missing, aliasable, unmapped = [], [], [], []
    for core in MANNY_CORE:
        if core in lower:
            matched.append(core)
        else:
            alias_hit = next((src for src, dst in COMMON_ALIASES.items()
                              if dst == core and src in lower), None)
            if alias_hit:
                aliasable.append({'manny': core, 'found_as': lower[alias_hit]})
            else:
                missing.append(core)
    for b in bones:
        lb = b.lower()
        if lb in MANNY_CORE:
            continue
        if lb in COMMON_ALIASES:
            continue
        if re.match(r'^(root|pelvis|spine|neck|head|clavicle|upperarm|lowerarm|hand|thigh|calf|foot|ball|'
                    r'thumb|index|middle|ring|pinky|.*_l$|.*_r$)', lb):
            continue
        unmapped.append(b)

    exact = len(matched) == len(MANNY_CORE)
    report['skeleton_comparison'] = {
        'manny_core_total': len(MANNY_CORE),
        'exact_match': matched,
        'alias_match': aliasable,
        'missing': missing,
        'unmapped_extra_bones': unmapped,
        'exact_match_ratio': round(len(matched) / len(MANNY_CORE), 3),
    }

    # ------------------------------------------------- 4. 结论
    if exact:
        report['verdict'].append('骨骼命名与 SK_Mannequin 核心骨完全一致 → 可直接指定现有 Skeleton，131 段动画零重定向。仍需实测一段走路动画确认无扭曲。')
    elif len(matched) >= len(MANNY_CORE) * 0.6:
        report['verdict'].append('骨骼大体对应但存在命名差异 → 走 Auto Retargeting（右键动画 → Retarget Anim Assets），或按 alias 表改名后重导。')
    else:
        report['verdict'].append('骨骼命名与 Manny 差异很大 → 必须走 IK Retargeter 重定向，且需要人工核对链映射。')

    if abs(height - TARGET_HEIGHT_CM) <= TOLERANCE_CM:
        report['verdict'].append(f'身高 {height:.1f}cm 在基准 {TARGET_HEIGHT_CM}±{TOLERANCE_CM}cm 内 → Capsule 与相机偏移按现有值微调即可。')
    else:
        diff = height - TARGET_HEIGHT_CM
        report['verdict'].append(f'身高 {height:.1f}cm 偏离基准 {TARGET_HEIGHT_CM}cm（差 {diff:+.1f}cm）→ 优先在导入时用 Import Uniform Scale 校正；若差值接近 100 倍则是单位问题。')
        report['blocking'].append(f'身高偏差 {diff:+.1f}cm 超出容差，接入前必须校正。')

    if 'sha256' not in main:
        report['verdict'].append('未能记录所选 FBX 的 sha256（文件可能被占用）。')

out = Path(unreal.Paths.project_dir()) / 'Docs/开发过程/验收记录/M8_ArtCheck.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

print('M8_ART_CHECK_DONE')
print('  FBX        :', report.get('chosen_fbx'))
print('  骨骼数     :', (report['mesh'] or {}).get('bone_count'))
print('  身高       :', (report['mesh'] or {}).get('height_cm'), 'cm')
print('  贴图       :', report['textures_found'])
print('  命名一致度 :', (report['skeleton_comparison'] or {}).get('exact_match_ratio'))
for v in report['verdict']:
    print('  ·', v)
for b in report['blocking']:
    print('  !! BLOCKING:', b)
print('  报告       :', out)
