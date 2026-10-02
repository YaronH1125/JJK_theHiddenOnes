"""Read-only saved binding/dependency audit. Run again after an Editor cold restart."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
editor_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert editor_world and editor_world.get_name()=='L_DojoArena'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
registry=unreal.AssetRegistryHelpers.get_asset_registry()
options=unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=False,
 include_searchable_names=False,include_hard_management_references=False,include_soft_management_references=False)
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
expected=['/Script/JJK_theHiddenOnes.'+n for n in ['CombatPoseConsumer','CombatRangedVisualConsumer','CombatAudioConsumer']]
assert sorted(x.get_path_name() for x in fd.feedback_consumer_classes)==sorted(expected)
assert fd.reaction_library and fd.feedback_profile and fd.feedback_profile.external_ranged_impact
roots=['/Game/Training/DA_Fighter_Ishigori']
graph={};todo=list(roots)
while todo:
    p=todo.pop()
    if p in graph or not p.startswith('/Game/'):continue
    deps=[str(x) for x in registry.get_dependencies(p,options)]
    graph[p]=deps;todo.extend(deps)
required=['/Game/Characters/Ishigori/Repaired/Feedback/DA_FB_Reaction_Ishigori',
 '/Game/CombatFeedback/Profiles/DA_CombatFeedback_Ishigori','/Game/CombatFeedback/Profiles/Audio/DA_CombatAudio',
 '/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual']
required+=['/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_'+n for n in ['Light','Heavy','Guard','GuardStart','GuardLoop','GuardEnd','SuperRecoil']]
required+=['/Game/CombatFeedback/VFX/PS_C_'+n for n in ['Hit_Frost','Guard_Laser','Immune_Laser','Expire_Laser','World_Sand']]
missing=[x for x in required if x not in graph]
all_feedback=[str(x.package_name) for x in registry.get_assets_by_path('/Game/CombatFeedback',recursive=True)]
unreachable=[x for x in all_feedback if x not in graph]
invalid=[x for x in graph if not unreal.EditorAssetLibrary.does_asset_exist(x)]
attacks=list(fd.combo_segments)+list(fd.kick_segments)+[fd.heavy_punch_definition,fd.heavy_kick_definition]
gameplay=[]
for a in attacks:
    gameplay.append(dict(asset=a.get_path_name(),damage=a.damage,trace_radius=a.trace_radius,
                         hit_effect=str(a.hit_effect),knockback=a.knockback_strength,knockdown=a.knockdown))
report={'status':'passed' if not missing and not unreachable and not invalid else 'failed',
 'map':'/Game/Maps/L_DojoArena','engine':unreal.SystemLibrary.get_engine_version(),
 'fighter':fd.get_path_name(),'classes':expected,'required_missing':missing,'unreachable_feedback_assets':unreachable,
 'missing_packages':invalid,'hard_dependency_count':len(graph),'graph':graph,'attacks':gameplay,
 'defaults':{n:unreal.SystemLibrary.get_console_variable_float_value(n) for n in ['JJK.Feedback.HitStop','JJK.Feedback.Reaction','JJK.Feedback.Audio','JJK.Feedback.AudioGain','JJK.Feedback.AudioLog','JJK.Feedback.Log','JJK.Feedback.RangedFX','JJK.Feedback.Camera','JJK.Feedback.CameraStrength','JJK.Feedback.HUD']},
 'cook':'not executed; hard-reference reachability does not replace packaged validation'}
path=Path(unreal.Paths.project_saved_dir())/'FeedbackA2/dependency-audit.json'
path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['graph','attacks']},ensure_ascii=False))
assert report['status']=='passed'
