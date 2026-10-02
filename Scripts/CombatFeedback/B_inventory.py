"""Read current character, attack timelines and reaction motion; never save assets.

Run from project root: python Scripts/ue_python.py Scripts/CombatFeedback/B_inventory.py
Output: Saved/FeedbackB/inventory.json (before making B assets).
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Characters/Ishigori/Repaired'
OUT = Path(unreal.Paths.project_saved_dir()) / 'FeedbackB'
OUT.mkdir(exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

def path(o):
    return o.get_path_name() if o else None

def vec(v):
    return [round(v.x, 4), round(v.y, 4), round(v.z, 4)]

cdo = unreal.get_default_object(unreal.load_class(None, '/Game/Training/BP_Fighter.BP_Fighter_C'))
mesh = cdo.get_editor_property('mesh')
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
result = dict(mesh=path(mesh.skeletal_mesh), skeleton=path(mesh.skeletal_mesh.skeleton),
              abp=path(mesh.anim_class), scale=vec(mesh.relative_scale3d),
              mesh_location=vec(mesh.relative_location), mesh_rotation=str(mesh.relative_rotation),
              reaction_library=path(fd.reaction_library), attacks={}, sequences={}, slots=[])
abp = unreal.load_asset(ROOT + '/Gameplay/IG_ABP_Unarmed')
node = unreal.load_object(None, abp.get_path_name() + ':AnimGraph.AnimGraphNode_Slot_0')
if node:
    result['slots'].append(str(node.get_editor_property('node')))

fields = ['trace_socket', 'damage', 'trace_radius', 'hit_stun_duration', 'guard_stun_duration',
          'window_start_time', 'window_end_time', 'combo_window_start_time', 'combo_window_end_time',
          'cancel_window_start_time', 'cancel_window_end_time', 'charge_hold_time',
          'knockback_strength', 'knockdown', 'hit_effect', 'grants_super_armor']
for name in ['A1', 'A2', 'A3', 'A4', 'Kick', 'Kick2', 'Kick3', 'HeavyPunch', 'HeavyKick']:
    d = unreal.load_asset('/Game/Training/DA_M3_' + name)
    m = d.montage
    result['attacks'][name] = {k: str(d.get_editor_property(k)) for k in fields}
    result['attacks'][name].update(montage=path(m), length=m.get_play_length(),
        tracks=[str(t) for t in m.get_editor_property('slot_anim_tracks')],
        blend_in=str(m.get_editor_property('blend_in')), blend_out=str(m.get_editor_property('blend_out')))

for asset in unreal.EditorAssetLibrary.list_assets(ROOT + '/Combat'):
    seq = unreal.load_asset(asset)
    if not isinstance(seq, unreal.AnimSequence):
        continue
    rows = []
    for i in range(61):
        t = seq.get_play_length() * i / 60
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, unreal.AnimPoseEvaluationOptions())
        row = {'t': round(t, 5)}
        for bone in ['root', 'pelvis', 'spine_03', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r']:
            row[bone] = vec(pose.get_bone_pose(bone, unreal.AnimPoseSpaces.WORLD).translation)
        rows.append(row)
    result['sequences'][seq.get_name()] = dict(path=path(seq), length=seq.get_play_length(),
        skeleton=path(seq.get_editor_property('skeleton')),
        root_motion=seq.get_editor_property('enable_root_motion'), samples=rows)
(OUT / 'inventory.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k != 'sequences'}, indent=2))
print('Sampled sequences:', len(result['sequences']))
