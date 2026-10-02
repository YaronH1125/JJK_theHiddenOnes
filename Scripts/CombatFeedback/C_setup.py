"""Temporarily bind C's consumer for isolated PIE; do not save the fighter asset.

Called by C_run.py while the editor is idle. Cleanup reloads only this test-dirty
fighter definition from its unchanged disk file after PIE.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
fd = unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
old_classes = list(fd.get_editor_property('feedback_consumer_classes'))
old_profile = fd.get_editor_property('feedback_profile')
perf = unreal.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
profile = unreal.new_object(unreal.CombatFeedbackProfile)
profile.set_editor_property('external_ranged_impact', True)
classes = list(old_classes)
consumer = unreal.CombatRangedVisualConsumer.static_class()
if consumer not in classes: classes.append(consumer)
fd.set_editor_property('feedback_profile', profile)
fd.set_editor_property('feedback_consumer_classes', classes)

out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackC'
out.mkdir(exist_ok=True)
state = {'old_classes': [x.get_path_name() for x in old_classes],
         'old_profile': old_profile.get_path_name() if old_profile else None,
         'test_classes': [x.get_path_name() for x in classes],
         'external_ranged_impact': profile.external_ranged_impact,
         'cvars': {n: unreal.SystemLibrary.get_console_variable_int_value(n) for n in
                   ['t.MaxFPS','JJK.Feedback.HitStop','JJK.Feedback.RangedFX']},
         'throttle': old_throttle}
(out / 'setup.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
print(json.dumps(state))
