"""Validate B assets and sample bone stability; does not save any assets.

python Scripts/ue_python.py Scripts/CombatFeedback/B_validate_assets.py
Output: Saved/FeedbackB/asset-validation.json. This is asset QA, not integration QA.
"""
import json
import math
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir())/'FeedbackB'
build = json.loads((out/'build.json').read_text(encoding='utf-8'))
report = dict(checks=[], motion={}, scope='asset-only')
expected = '/Game/Characters/Ishigori/Repaired/SK_Ishigori_Repaired_Skeleton.SK_Ishigori_Repaired_Skeleton'
def check(name,ok,detail=None):
    report['checks'].append(dict(name=name,passed=bool(ok),detail=detail))
def distance(a,b):
    return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
def v(p): return [p.x,p.y,p.z]
for path in build['assets']:
    asset = unreal.load_asset(path)
    check(path+' exists',bool(asset))
    if isinstance(asset,unreal.AnimSequenceBase):
        check(asset.get_name()+' repaired skeleton',asset.get_editor_property('skeleton').get_path_name()==expected)
        check(asset.get_name()+' finite length',0<asset.get_play_length()<1.)
    if isinstance(asset,unreal.AnimMontage):
        tracks = asset.get_editor_property('slot_anim_tracks')
        check(asset.get_name()+' DefaultSlot',len(tracks)==1 and str(tracks[0].slot_name)=='DefaultSlot')
        check(asset.get_name()+' no gameplay notifies',len(unreal.AnimationLibrary.get_animation_notify_events(asset))==0)
for name,entry in build['sequences'].items():
    seq=unreal.load_asset(entry['path'])
    check(name+' no root motion or forced root lock',not seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('force_root_lock'))
    samples=[]
    for i in range(61):
        t=seq.get_play_length()*i/60
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,t,unreal.AnimPoseEvaluationOptions())
        samples.append({b:v(pose.get_bone_pose(b,unreal.AnimPoseSpaces.WORLD).translation) for b in ['root','pelvis','head','hand_l','hand_r','foot_l','foot_r']})
    drift={b:max(distance(s[b],samples[0][b])*1.9369 for s in samples) for b in samples[0]}
    report['motion'][name]=dict(drift_cm=drift,first=samples[0],last=samples[-1],peak=samples[round(entry['peak']/seq.get_play_length()*60)])
    check(name+' finite transforms',all(math.isfinite(c) for s in samples for p in s.values() for c in p))
    check(name+' planted feet/root/pelvis (<0.1cm)',all(drift[b]<.1 for b in ['root','pelvis','foot_l','foot_r']),drift)
    ref=unreal.AnimPoseExtensions.get_anim_pose_at_time(unreal.load_asset(entry['reference']),0.,unreal.AnimPoseEvaluationOptions())
    end_error=max(distance(samples[-1][b],v(ref.get_bone_pose(b,unreal.AnimPoseSpaces.WORLD).translation))*1.9369 for b in samples[0])
    check(name+' returns to reference (<0.1cm)',end_error<.1,end_error)
lib=unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/DA_FB_Reaction_Ishigori')
check('three distinct reaction slots',len({lib.light,lib.heavy,lib.guard})==3 and all([lib.light,lib.heavy,lib.guard]))
check('direction slots empty preserve tier fallback',all(lib.get_editor_property(n) is None for n in ['left','right','back']))
report['status']='passed' if all(c['passed'] for c in report['checks']) else 'failed'
(out/'asset-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(status=report['status'],checks=len(report['checks']),failed=[c for c in report['checks'] if not c['passed']],motion=report['motion']),indent=2))
