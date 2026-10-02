"""Validate C-owned profile, hard references, and untouched melee effect slots.

python Scripts/ue_python.py Scripts/CombatFeedback/C_validate_assets.py
Read-only; writes Saved/FeedbackC/asset-validation.json.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC'
profile = unreal.load_asset('/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual')
consumer = unreal.get_default_object(unreal.CombatRangedVisualConsumer)
checks = []
def check(name, ok, detail=None):
    checks.append({'name': name, 'passed': bool(ok), 'detail': str(detail) if detail is not None else None})
check('Native consumer CDO hard references visual profile', consumer.visual_profile == profile,
      consumer.visual_profile.get_path_name() if consumer.visual_profile else None)
for name in ['mobile_hit','super_hit','domain_hit','guard','immune','world_impact','expire']:
    cue = profile.get_editor_property(name)
    effect = cue.get_editor_property('effect')
    check(name + ' uses a C-owned effect', effect and effect.get_path_name().startswith('/Game/CombatFeedback/VFX/'),
          effect.get_path_name() if effect else None)
    check(name + ' has a bounded lifetime', 0.05 <= cue.get_editor_property('max_life') <= .55,
          cue.get_editor_property('max_life'))
for name in ['A1','A2','A3','A4','Kick','Kick2','Kick3','HeavyPunch','HeavyKick']:
    attack = unreal.load_asset('/Game/Training/DA_M3_' + name)
    check(name + ' melee HitEffect stays empty', not attack.get_editor_property('hit_effect'),
          attack.get_editor_property('hit_effect'))
report = {'status': 'passed' if all(x['passed'] for x in checks) else 'failed', 'checks': checks}
(out / 'asset-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'status': report['status'], 'count': len(checks),
                  'failed': [x for x in checks if not x['passed']]}))
