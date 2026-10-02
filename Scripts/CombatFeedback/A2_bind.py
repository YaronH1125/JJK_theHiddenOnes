"""A's authorized production wiring. Saves only the A profile and fighter definition."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
assert fd
profile_path = '/Game/CombatFeedback/Profiles/DA_CombatFeedback_Ishigori'
profile = unreal.load_asset(profile_path)
if not profile:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.CombatFeedbackProfile)
    profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_CombatFeedback_Ishigori', '/Game/CombatFeedback/Profiles', unreal.CombatFeedbackProfile, factory)
assets = ['/Game/CombatFeedback/Profiles/Audio/DA_CombatAudio', '/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual']
assets += ['/Game/Characters/Ishigori/Repaired/Feedback/IG_AM_FB_' + n for n in ['GuardStart', 'GuardLoop', 'GuardEnd', 'SuperRecoil']]
loaded = [unreal.load_asset(p) for p in assets]
assert all(loaded)
profile.set_editor_property('consumer_assets', loaded)
profile.set_editor_property('external_ranged_impact', True)
lib = unreal.load_asset('/Game/Characters/Ishigori/Repaired/Feedback/DA_FB_Reaction_Ishigori')
assert lib and lib.light and lib.heavy and lib.guard
classes = list(fd.feedback_consumer_classes)
for consumer in (unreal.CombatPoseConsumer, unreal.CombatRangedVisualConsumer, unreal.CombatAudioConsumer):
    cls = consumer.static_class()
    if cls not in classes: classes.append(cls)
assert len(classes) == len(set(x.get_path_name() for x in classes))
fd.set_editor_property('feedback_consumer_classes', classes)
fd.set_editor_property('reaction_library', lib)
fd.set_editor_property('feedback_profile', profile)
assert unreal.EditorAssetLibrary.save_loaded_asset(profile, False)
assert unreal.EditorAssetLibrary.save_loaded_asset(fd, False)
result = {'fighter': fd.get_path_name(), 'profile': profile.get_path_name(), 'reaction_library': lib.get_path_name(),
          'consumers': [x.get_path_name() for x in classes], 'consumer_assets': assets,
          'external_ranged_impact': profile.external_ranged_impact,
          'initial_health': fd.initial_health, 'initial_energy': fd.initial_energy, 'initial_cursed_energy': fd.initial_cursed_energy}
(Path(unreal.Paths.project_saved_dir())/'FeedbackA2/binding.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result))
