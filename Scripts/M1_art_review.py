"""Isolated legacy FBX trial import; never opens or writes the legacy project.
Imports into /Game/Training/ArtReview and records measured skeleton/animation data.
"""
import hashlib
import json
from pathlib import Path
import unreal

assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
folder = '/Game/Training/ArtReview'
source = Path('F:/GameStudy/JJKDemo/sll')
eal = unreal.EditorAssetLibrary
report = {'source': str(source), 'engine': unreal.SystemLibrary.get_engine_version(), 'files': [], 'animations': []}
for p in sorted(source.glob('*.fbx')):
    report['files'].append({'name': p.name, 'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})

def import_fbx(filename, name, skeleton=None):
    path = folder + '/' + name
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = True
    options.create_physics_asset = False
    options.import_mesh = skeleton is None
    options.import_animations = skeleton is not None
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeleton is None else unreal.FBXImportType.FBXIT_ANIMATION
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    if skeleton:
        options.skeleton = skeleton
    task = unreal.AssetImportTask()
    task.filename = str(source / filename)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = [eal.load_asset(p) for p in task.imported_object_paths]
    expected = unreal.SkeletalMesh if skeleton is None else unreal.AnimSequence
    return next(a for a in assets if isinstance(a, expected))

mesh = import_fbx('T.fbx', 'SK_Ishigori_Review')
skeleton = mesh.get_editor_property('skeleton')
pose = skeleton.get_reference_pose()
bones = [str(n) for n in pose.get_bone_names()]
bounds = mesh.get_bounds()
def xyz(vector):
    return [round(vector.x, 4), round(vector.y, 4), round(vector.z, 4)]
foot = pose.get_bone_pose('LeftFoot', unreal.AnimPoseSpaces.WORLD).translation
toe = pose.get_bone_pose('LeftToeBase', unreal.AnimPoseSpaces.WORLD).translation
report['facing_reference'] = {'left_foot':xyz(foot), 'left_toe':xyz(toe),
                            'foot_to_toe':xyz(toe-foot), 'method':'reference-pose WORLD/component coordinates'}
report['mesh'] = {'path':mesh.get_path_name(), 'skeleton':skeleton.get_path_name(), 'bone_count':len(bones),
                  'bones':bones, 'bounds_origin':str(bounds.origin), 'bounds_extent':str(bounds.box_extent),
                  'height_cm':bounds.box_extent.z * 2, 'material_slots':len(mesh.materials)}
head = next((b for b in bones if b.lower().endswith('head')), None)
report['head_bone'] = head
if head:
    if not mesh.find_socket('Muzzle_Head_Review'):
        socket = unreal.new_object(unreal.SkeletalMeshSocket, outer=mesh)
        socket.set_socket_parent(mesh, head)
        mesh.modify()
        mesh.add_socket(socket, False)
        before_name = str(socket.socket_name)
        report['socket_rename_result'] = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).rename_socket(mesh, before_name, 'Muzzle_Head_Review')
    socket = mesh.find_socket('Muzzle_Head_Review')
    report['socket'] = {'name':str(socket.socket_name), 'bone':str(socket.bone_name), 'note':'Feasibility only; muzzle offset and forward axis require M6 visual calibration'} if socket else {'error':'Socket rename failed; head bone exists'}
    assert eal.save_loaded_asset(mesh, only_if_is_dirty=False)
assert eal.save_loaded_asset(skeleton, only_if_is_dirty=False)
for filename, name in [('Standing Idle.fbx','A_Ishigori_Idle_Review'),('Walking.fbx','A_Ishigori_Walk_Review'),('Running.fbx','A_Ishigori_Run_Review')]:
    try:
        anim = import_fbx(filename, name, skeleton)
        report['animations'].append({'path':anim.get_path_name(), 'skeleton':anim.get_editor_property('skeleton').get_path_name(),
                                     'duration':anim.get_play_length(), 'compatible':anim.get_editor_property('skeleton') == skeleton})
    except Exception as e:
        report['animations'].append({'file':filename, 'error':str(e)})
placeholder = eal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
attack = eal.load_asset('/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01')
report['m2_placeholder'] = {'mesh':placeholder.get_path_name(), 'skeleton':placeholder.skeleton.get_path_name(),
                          'attack':attack.get_path_name(), 'attack_skeleton':attack.get_editor_property('skeleton').get_path_name(),
                          'attack_duration':attack.get_play_length(), 'same_skeleton':placeholder.skeleton == attack.get_editor_property('skeleton')}
report['missing'] = ['combat animations / retargeting', 'materials and textures verification', 'head muzzle offset calibration', 'collision and physics asset', 'in-arena proportions and facing review']
out = Path(unreal.Paths.project_dir()) / 'Docs/开发过程/验收记录/M1_ArtReview.json'
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('M1_ART_REVIEW', report)

