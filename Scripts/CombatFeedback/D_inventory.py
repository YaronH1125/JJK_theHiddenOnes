"""Read all project sound assets, export existing waves for analysis; no source saves.

From project root, idle Editor: python Scripts/ue_python.py Scripts/CombatFeedback/D_inventory.py
Evidence and WAV candidates: Saved/FeedbackD/inventory.json and source-wav/.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(unreal.Paths.project_saved_dir()) / 'FeedbackD'
wav_dir = out / 'source-wav'
wav_dir.mkdir(parents=True, exist_ok=True)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(synchronous_search=True)
rows = []
mix = []
for data in registry.get_assets_by_path('/Game', recursive=True):
    kind = str(data.asset_class_path.asset_name)
    if kind in ['SoundClass', 'SoundMix', 'SoundSubmix', 'SoundConcurrency', 'SoundAttenuation']:
        mix.append({'path': str(data.package_name), 'class': kind})
    if kind not in ['SoundWave', 'SoundCue', 'MetaSoundSource']:
        continue
    obj = data.get_asset()
    row = {'path': obj.get_path_name(), 'class': kind}
    for prop in ['duration', 'num_channels', 'sample_rate', 'looping', 'volume', 'pitch', 'sound_class_object']:
        try:
            v = obj.get_editor_property(prop)
            row[prop] = v.get_path_name() if isinstance(v, unreal.Object) else v
        except Exception:
            pass
    if kind == 'SoundWave':
        task = unreal.AssetExportTask()
        task.object = obj
        task.filename = str(wav_dir / (obj.get_name() + '.wav'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        row['exported'] = unreal.Exporter.run_asset_export_task(task)
        row['wav'] = task.filename
    rows.append(row)
result = {'sounds': rows, 'existing_mix_assets': mix}
(out / 'inventory.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
