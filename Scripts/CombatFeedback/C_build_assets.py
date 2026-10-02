"""Duplicate only vendor VFX into C-owned folders and build the hard-reference cue profile.

Run from project root outside PIE, after building the C++ consumer:
  python Scripts/ue_python.py Scripts/CombatFeedback/C_build_assets.py
Writes /Game/CombatFeedback/VFX and /Game/CombatFeedback/Profiles/Visual only.
Reruns update the same C-owned assets. Vendor packages and shared fighter assets are read-only.
Evidence: Saved/FeedbackC/build.json.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
vfx = '/Game/CombatFeedback/VFX'
profiles = '/Game/CombatFeedback/Profiles/Visual'
for directory in [vfx, profiles]:
    assert unreal.EditorAssetLibrary.make_directory(directory) or unreal.EditorAssetLibrary.does_directory_exist(directory)

source = '/Game/GoodParticleBeamAndRay/Particles/Impact/'
copies = {
    'PS_C_Hit_Frost': 'PS_GPBAR_Frost_Impact',
    'PS_C_Guard_Laser': 'PS_GPBAR_Laser_Impact',
    'PS_C_World_Sand': 'PS_GPBAR_Sand_Impact',
    'PS_C_Immune_Laser': 'PS_GPBAR_Laser_Impact',
    'PS_C_Expire_Laser': 'PS_GPBAR_Laser_Impact',
}
result = {'source_to_derived': {}, 'profile': None, 'cues': {}}
loaded = {}
for dest_name, source_name in copies.items():
    src = source + source_name
    dest = vfx + '/' + dest_name
    assert unreal.EditorAssetLibrary.does_asset_exist(src), src
    if not unreal.EditorAssetLibrary.does_asset_exist(dest):
        assert unreal.EditorAssetLibrary.duplicate_asset(src, dest), (src, dest)
    effect = unreal.load_asset(dest)
    assert isinstance(effect, unreal.ParticleSystem) and effect.get_path_name().startswith(vfx + '/'), dest
    assert unreal.EditorAssetLibrary.save_loaded_asset(effect, only_if_is_dirty=False), dest
    loaded[dest_name] = effect
    result['source_to_derived'][src] = result['source_to_derived'].get(src, []) + [dest]

dest = profiles + '/DA_RangedVisual'
profile = unreal.load_asset(dest) if unreal.EditorAssetLibrary.does_asset_exist(dest) else None
if not profile:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.CombatRangedVisualProfile)
    profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_RangedVisual', profiles,
                                                                     unreal.CombatRangedVisualProfile, factory)
assert profile and isinstance(profile, unreal.CombatRangedVisualProfile)

config = {
    'mobile_hit': ('PS_C_Hit_Frost', .73, .42),
    'super_hit': ('PS_C_Hit_Frost', 1.05, .54),
    'domain_hit': ('PS_C_Hit_Frost', .48, .34),
    'guard': ('PS_C_Guard_Laser', .56, .27),
    'immune': ('PS_C_Immune_Laser', .21, .17),
    'world_impact': ('PS_C_World_Sand', .66, .38),
    'expire': ('PS_C_Expire_Laser', .12, .16),
}
for field, (name, scale, life) in config.items():
    cue = unreal.CombatRangedVisualCue()
    cue.set_editor_property('effect', loaded[name])
    cue.set_editor_property('scale', scale)
    cue.set_editor_property('max_life', life)
    profile.set_editor_property(field, cue)
    assert profile.get_editor_property(field).get_editor_property('effect') == loaded[name], field
    result['cues'][field] = {'effect': loaded[name].get_path_name(), 'scale': scale, 'max_life': life}
assert unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
result['profile'] = profile.get_path_name()

out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC'
out.mkdir(exist_ok=True)
(out / 'build.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
