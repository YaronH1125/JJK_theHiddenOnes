"""Build B-owned feedback assets from existing repaired animation samples.

Run outside PIE: python Scripts/ue_python.py Scripts/CombatFeedback/B_build_assets.py
Writes only /Game/Characters/Ishigori/Repaired/Feedback and Saved/FeedbackB/build.json.
Reruns update these same B-owned assets; never write source clips/shared data assets.
The baked upper-body rotations keep the reference lower body planted. These are
ordinary non-additive sequences for the existing DefaultSlot, not runtime layers.
"""
import json
import math
from pathlib import Path
import unreal

ROOT = '/Game/Characters/Ishigori/Repaired'
COM = ROOT + '/Combat'
DEST = ROOT + '/Feedback'
OUT = Path(unreal.Paths.project_saved_dir()) / 'FeedbackB'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
tools = unreal.AssetToolsHelpers.get_asset_tools()
skel = unreal.load_asset(ROOT + '/SK_Ishigori_Repaired_Skeleton')
idle = unreal.load_asset(COM + '/IG_KB_Idle_2')
guard = unreal.load_asset(COM + '/IG_KB_Block_Loop')
opts = unreal.AnimPoseEvaluationOptions()
LOCAL = unreal.AnimPoseSpaces.LOCAL
report = {'version': 'FB-20260930-v1', 'assets': [], 'sequences': {}, 'montages': {}}

def smooth(x):
    x = max(0., min(1., x))
    return x*x*(3-2*x)

def slerp(a, b, w):
    aa, bb = [a.x,a.y,a.z,a.w], [b.x,b.y,b.z,b.w]
    dot = sum(x*y for x,y in zip(aa,bb))
    if dot < 0:
        bb = [-x for x in bb]; dot = -dot
    if dot > .9995:
        q = [x+(y-x)*w for x,y in zip(aa,bb)]
    else:
        angle = math.acos(max(-1., min(1., dot)))
        q = [(math.sin((1-w)*angle)*x+math.sin(w*angle)*y)/math.sin(angle) for x,y in zip(aa,bb)]
    norm = math.sqrt(sum(x*x for x in q))
    return unreal.Quat(*[x/norm for x in q])

def save(obj):
    assert obj.get_path_name().startswith(DEST + '/'), obj
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False)
    report['assets'].append(obj.get_path_name())

def sequence(name, source, base, frames, peak, mode):
    dest = DEST + '/IG_AS_FB_' + name
    obj = unreal.load_asset(dest) if unreal.EditorAssetLibrary.does_asset_exist(dest) else None
    if not obj:
        factory = unreal.AnimSequenceFactory()
        factory.target_skeleton = skel
        factory.preview_skeletal_mesh = unreal.load_asset(ROOT + '/SK_Ishigori_Repaired')
        obj = tools.create_asset('IG_AS_FB_'+name, DEST, unreal.AnimSequence, factory)
    ref = unreal.AnimPoseExtensions.get_anim_pose_at_time(base, 0., opts)
    bones = list(ref.get_bone_names())
    keys = {str(b): ([], [], []) for b in bones}
    for i in range(frames+1):
        t = i / 60.
        if mode == 'light':
            st = .11 + (.26-.11)*min(t/peak, 1.) if t <= peak else .26+(.833333-.26)*smooth((t-peak)/(frames/60.-peak))
            weight = 1.
        else:
            st = (.12 if mode == 'heavy' else .10) + ((.28 if mode == 'heavy' else .34)-(.12 if mode == 'heavy' else .10))*smooth(t/peak)
            weight = 1. if t <= peak else 1.-smooth((t-peak)/(frames/60.-peak))
            if mode == 'recoil': weight *= .65
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, st, opts)
        for bone in bones:
            n = str(bone)
            b = ref.get_bone_pose(bone, LOCAL)
            p = pose.get_bone_pose(bone, LOCAL)
            upper = not n.startswith(('root', 'pelvis', 'thigh', 'calf', 'foot', 'ball', 'ik_'))
            if mode == 'recoil': upper = n.startswith(('spine_', 'neck_', 'head'))
            positions, rotations, scales = keys[n]
            positions.append(b.translation)
            rotations.append(slerp(b.rotation, p.rotation, weight) if upper else b.rotation)
            scales.append(b.scale3d)
    controller = obj.controller
    controller.open_bracket('B baked feedback from repaired clips', False)
    try:
        controller.set_frame_rate(unreal.FrameRate(60, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(frames), False)
        for n, (p,r,s) in keys.items():
            controller.add_bone_curve(n, False)
            assert controller.set_bone_track_keys(n,p,r,s,False), n
    finally:
        controller.close_bracket(False)
    obj.set_editor_property('enable_root_motion', False)
    # Match the source clips. Root translation is already baked constant.
    obj.set_editor_property('force_root_lock', False)
    save(obj)
    report['sequences'][name] = dict(path=obj.get_path_name(), source=source.get_path_name(),
        reference=base.get_path_name(), duration=obj.get_play_length(), frames=frames, peak=peak,
        mode=mode, note='fixed reference translations and lower body; sampled upper rotations')
    return obj

clips = {
    'Light': sequence('Light', unreal.load_asset(COM+'/IG_KB_Hit_p_MidFront_Weak'), idle, 20, .05, 'light'),
    'Heavy': sequence('Heavy', unreal.load_asset(COM+'/IG_KB_Hit_m_MidFront_Stagger'), idle, 28, .075, 'heavy'),
    # Use an even number of 60 Hz frames: UE's default 30 Hz compression must
    # land the final sample on a whole frame (17/60 triggered an engine assert).
    'Guard': sequence('Guard', unreal.load_asset(COM+'/IG_KB_Block_Single'), guard, 18, .065, 'guard'),
    'SuperRecoil': sequence('SuperRecoil', unreal.load_asset(COM+'/IG_KB_Block_Single'), idle, 22, .065, 'recoil'),
}

def montage(name, seq, blend_in, blend_out):
    dest = DEST + '/IG_AM_FB_' + name
    m = unreal.load_asset(dest) if unreal.EditorAssetLibrary.does_asset_exist(dest) else None
    if not m:
        factory = unreal.AnimMontageFactory()
        factory.target_skeleton = skel
        m = tools.create_asset('IG_AM_FB_'+name, DEST, unreal.AnimMontage, factory)
    seg = unreal.AnimSegment()
    seg.set_editor_property('anim_reference', seq)
    seg.import_text(f'(StartPos=0,AnimStartTime=0,AnimEndTime={seq.get_play_length()},AnimPlayRate=1,LoopingCount=1,CachedPlayLength={seq.get_play_length()})')
    track = unreal.AnimTrack(); track.set_editor_property('anim_segments', [seg])
    slot = unreal.SlotAnimationTrack(); slot.set_editor_property('slot_name', 'DefaultSlot'); slot.set_editor_property('anim_track', track)
    m.set_editor_property('slot_anim_tracks', [slot])
    for prop, value in [('blend_in',blend_in), ('blend_out',blend_out)]:
        blend = m.get_editor_property(prop); blend.set_editor_property('blend_time', value); m.set_editor_property(prop,blend)
    save(m)
    unreal.EditorLoadingAndSavingUtils.reload_packages([m.get_outer()], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    m = unreal.load_asset(dest)
    assert abs(m.get_play_length()-seq.get_play_length()) < .002
    save(m)
    report['montages'][name] = dict(path=m.get_path_name(), sequence=seq.get_path_name(), duration=m.get_play_length(), slot='DefaultSlot', blend_in=blend_in, blend_out=blend_out)
    return m

montages = {n:montage(n,s,.0 if n != 'SuperRecoil' else .025,.08 if n != 'Guard' else .05) for n,s in clips.items()}
for n,seq,bi,bo in [('GuardStart','Block_Start',.06,.05), ('GuardLoop','Block_Loop',.05,.06), ('GuardEnd','Block_End',.05,.08)]:
    montage(n,unreal.load_asset(COM+'/IG_KB_'+seq),bi,bo)
lib_path = DEST + '/DA_FB_Reaction_Ishigori'
library = unreal.load_asset(lib_path) if unreal.EditorAssetLibrary.does_asset_exist(lib_path) else None
if not library:
    factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.CombatReactionLibrary)
    library = tools.create_asset('DA_FB_Reaction_Ishigori',DEST,unreal.CombatReactionLibrary,factory)
for n in ['Light','Heavy','Guard']:
    library.set_editor_property(n.lower(),montages[n])
for n in ['left','right','back']:
    library.set_editor_property(n,None)
save(library)
report['assets'] = sorted(set(report['assets']))
(OUT/'build.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
