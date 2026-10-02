"""Read the active ranged VFX references and source Cascade settings; save nothing.

Run from project root with an idle editor:
  python Scripts/ue_python.py Scripts/CombatFeedback/C_inventory.py
Writes Saved/FeedbackC/inventory.json. Third-party packages remain read-only.
"""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC'
out.mkdir(exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')

def path(obj):
    return obj.get_path_name() if obj else None

report = {'fighter': path(fd), 'muzzle_socket': str(fd.muzzle_socket), 'blast': {}, 'effects': {}}
for name in ['mobile_blast', 'super_blast']:
    cfg = fd.get_editor_property(name)
    report['blast'][name] = {key: str(cfg.get_editor_property(key)) for key in
        ['trail_effect_scale', 'impact_effect_scale', 'impact_effect_life', 'beam_width_min', 'beam_width_max']}
    report['blast'][name].update({key: path(cfg.get_editor_property(key)) for key in
        ['trail_effect', 'impact_effect', 'beam_effect']})

assets = [v[key] for v in report['blast'].values() for key in ['trail_effect', 'impact_effect', 'beam_effect'] if v[key]]
assets += ['/Game/GoodParticleBeamAndRay/Particles/Impact/PS_GPBAR_Frost_Impact',
           '/Game/GoodParticleBeamAndRay/Particles/Impact/PS_GPBAR_Laser_Impact',
           '/Game/GoodParticleBeamAndRay/Particles/Impact/PS_GPBAR_Sand_Impact']
for ref in sorted(set(assets)):
    effect = unreal.load_asset(ref)
    if not effect:
        report['effects'][ref] = {'missing': True}
        continue
    row = {'class': effect.get_class().get_name(), 'emitters': []}
    if isinstance(effect, unreal.ParticleSystem):
        # UE 5.8 protects the Emitters property from Python. Enumerate loaded
        # package subobjects without changing or resaving the vendor package.
        prefix = effect.get_path_name() + ':'
        for obj in unreal.ObjectIterator():
            try:
                if obj.get_path_name().startswith(prefix):
                    row['emitters'].append({'name': obj.get_name(), 'class': obj.get_class().get_name()})
            except Exception:
                pass
    report['effects'][ref] = row

(out / 'inventory.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
