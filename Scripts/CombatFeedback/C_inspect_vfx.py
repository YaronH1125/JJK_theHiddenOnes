"""Read Cascade loop, duration and local-space flags through C's C++ diagnostics.

Run from project root with idle Editor:
  python Scripts/ue_python.py Scripts/CombatFeedback/C_inspect_vfx.py
Read-only. Writes Saved/FeedbackC/vfx-inspection.json.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
profile=unreal.load_asset('/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual')
fields=['mobile_hit','super_hit','domain_hit','guard','immune','world_impact','expire']
report={}
for field in fields:
    cue=profile.get_editor_property(field)
    effect=cue.get_editor_property('effect')
    assert effect, field
    report[field]={'effect':effect.get_path_name(),'scale':cue.get_editor_property('scale'),
                   'runtime_max_life':cue.get_editor_property('max_life'),
                   'source_flags':list(unreal.CombatRangedVisualInspection.describe_cascade(effect))}
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
report['legacy']={}
for name in ['mobile_blast','super_blast']:
    cfg=fd.get_editor_property(name)
    beam=cfg.get_editor_property('beam_effect')
    beam=beam.load_synchronous() if hasattr(beam,'load_synchronous') else beam
    trail=cfg.get_editor_property('trail_effect')
    report['legacy'][name]={'beam':beam.get_path_name() if beam else None,
                            'beam_flags':list(unreal.CombatRangedVisualInspection.describe_cascade(beam)) if beam else [],
                            'trail':str(trail) if trail else None,
                            'impact':str(cfg.get_editor_property('impact_effect'))}
out=Path(unreal.Paths.project_saved_dir())/'FeedbackC'
out.mkdir(exist_ok=True)
(out/'vfx-inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'effect':v['effect'],'emitter_count':len(v['source_flags'])-1,
                     'runtime_max_life':v['runtime_max_life']} for k,v in report.items() if k!='legacy'},ensure_ascii=False))
