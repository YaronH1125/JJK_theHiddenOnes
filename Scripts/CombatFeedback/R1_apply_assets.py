"""Apply the authorized first human-feedback revision, outside PIE.

Run: python Scripts/ue_python.py Scripts/CombatFeedback/R1_apply_assets.py
Writes project feedback profiles, derived particle effects and Heavy reaction,
plus the fighter's two beam asset references. Audio, attack/damage/resource
values and vendor packages stay unchanged.
Evidence: Saved/FeedbackRevisionR1/assets.json.
"""
import json
import math
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackRevisionR1'
out.mkdir(exist_ok=True)
report = {'version': 'FR1-20261001', 'saved': []}

def save(asset):
    assert asset.get_path_name().startswith(('/Game/CombatFeedback/', '/Game/Characters/Ishigori/Repaired/Feedback/', '/Game/Training/DA_Fighter_Ishigori.'))
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report['saved'].append(asset.get_path_name())

profile = unreal.load_asset('/Game/CombatFeedback/Profiles/DA_CombatFeedback_Ishigori')
for key, value in [('light_stop', .022), ('finisher_stop', .060), ('heavy_stop', .090)]:
    profile.set_editor_property(key, value)
assert abs(profile.continuous_stop_limit - .100) < .0001
save(profile)
report['stops'] = {key: profile.get_editor_property(key) for key in ['light_stop', 'medium_stop', 'finisher_stop', 'heavy_stop', 'continuous_stop_limit']}

visual = unreal.load_asset('/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual')
# Keep the established result/strength mapping. MaxLife now means emission time;
# FadeLife is the bounded natural tail, rather than an abrupt component deletion.
for field, tail in [('mobile_hit', .45), ('super_hit', .60), ('domain_hit', .30),
                    ('guard', .30), ('immune', .20), ('world_impact', .45), ('expire', .20)]:
    cue = visual.get_editor_property(field)
    cue.set_editor_property('fade_life', tail)
    visual.set_editor_property(field, cue)
save(visual)
report['visual'] = {key: {'emission': visual.get_editor_property(key).max_life,
                         'tail': visual.get_editor_property(key).fade_life}
                    for key in ['mobile_hit', 'super_hit', 'world_impact']}

# The native helper can access protected Cascade arrays; it refuses vendor
# packages. Existing particles fade via both color and alpha over normalized age.
for name, life in [('PS_C_Hit_Frost', .28), ('PS_C_World_Sand', .30),
                   ('PS_C_Guard_Laser', .24), ('PS_C_Immune_Laser', .18), ('PS_C_Expire_Laser', .18)]:
    effect=unreal.load_asset('/Game/CombatFeedback/VFX/'+name)
    assert unreal.CombatRangedVisualInspection.prepare_natural_fade(effect,life,False)
    save(effect)

beam_path='/Game/CombatFeedback/VFX/PS_R1_Beam_Frost'
if not unreal.EditorAssetLibrary.does_asset_exist(beam_path):
    assert unreal.EditorAssetLibrary.duplicate_asset('/Game/GoodParticleBeamAndRay/Particles/Beam/PS_GPBAR_Frost',beam_path)
beam=unreal.load_asset(beam_path)
assert unreal.CombatRangedVisualInspection.prepare_natural_fade(beam, .3, True)
save(beam)
fighter=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
for field in ['mobile_blast','super_blast']:
    config=fighter.get_editor_property(field)
    config.set_editor_property('beam_effect',beam)
    fighter.set_editor_property(field,config)
save(fighter)
refs=list(profile.consumer_assets)
if beam not in refs:refs.append(beam)
profile.set_editor_property('consumer_assets',refs);save(profile)
report['beam']={'derived':beam_path,'source_unchanged':True,'fade_parameters':['FeedbackBeamFade','FeedbackBeamFadeAlpha']}

root = '/Game/Characters/Ishigori/Repaired'
source = unreal.load_asset(root + '/Combat/IG_KB_Hit_m_MidFront_Stagger')
idle = unreal.load_asset(root + '/Combat/IG_KB_Idle_2')
sequence = unreal.load_asset(root + '/Feedback/IG_AS_FB_Heavy')
opts = unreal.AnimPoseEvaluationOptions()
local = unreal.AnimPoseSpaces.LOCAL
reference = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0., opts)
bones = list(reference.get_bone_names())
keys = {str(b): ([], [], []) for b in bones}

def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3.-2.*t)

def slerp(a, b, weight):
    aa, bb = [a.x, a.y, a.z, a.w], [b.x, b.y, b.z, b.w]
    dot = sum(x*y for x, y in zip(aa, bb))
    if dot < 0.: bb = [-x for x in bb]; dot = -dot
    if dot > .9995: values = [x+(y-x)*weight for x, y in zip(aa, bb)]
    else:
        angle = math.acos(max(-1., min(1., dot)))
        values = [(math.sin((1.-weight)*angle)*x + math.sin(weight*angle)*y)/math.sin(angle)
                  for x, y in zip(aa, bb)]
    length = math.sqrt(sum(v*v for v in values))
    return unreal.Quat(*[v/length for v in values])

# Put a readable impact pose on frame zero, so local hit stop holds the reaction
# itself. Preserve the old 28/60 duration, fixed root and fixed lower body.
frames, peak = 28, .050
for i in range(frames+1):
    t = i/60.
    sample_time = .240 + .100*smooth(t/peak)
    weight = 1.20 if t <= peak else 1.20*(1.-smooth((t-peak)/(frames/60.-peak)))
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, sample_time, opts)
    for bone in bones:
        name = str(bone)
        base = reference.get_bone_pose(bone, local)
        sampled = pose.get_bone_pose(bone, local)
        upper = not name.startswith(('root', 'pelvis', 'thigh', 'calf', 'foot', 'ball', 'ik_'))
        p, r, s = keys[name]
        p.append(base.translation)
        r.append(slerp(base.rotation, sampled.rotation, weight) if upper else base.rotation)
        s.append(base.scale3d)
controller = sequence.controller
controller.open_bracket('R1 readable heavy impact pose with fixed lower body', False)
try:
    controller.set_frame_rate(unreal.FrameRate(60, 1), False)
    controller.set_number_of_frames(unreal.FrameNumber(frames), False)
    for name, (positions, rotations, scales) in keys.items():
        controller.add_bone_curve(name, False)
        assert controller.set_bone_track_keys(name, positions, rotations, scales, False)
finally:
    controller.close_bracket(False)
save(sequence)
report['heavy'] = {'path': sequence.get_path_name(), 'duration': sequence.get_play_length(),
                   'frame_zero_source': .240, 'upper_rotation_weight': 1.20,
                   'fixed_root_lower_body': True}
(out/'assets.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
