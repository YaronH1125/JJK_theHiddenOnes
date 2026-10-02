"""Read-only D handoff verification; writes evidence only to Saved/FeedbackD.

python Scripts/CombatFeedback/D_finalize.py
Run after D_run.py cleanup and Editor release. Requires baseline-hashes.json and
handoff-processes.json, captured at the start and end of this D session.
Does not stage files, save shared assets, change settings, or claim human QA/Cook.
"""
import datetime
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/FeedbackD'


def read(name):
    return json.loads((out / name).read_text(encoding='utf-8-sig'))


def write(name, value):
    (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


baseline = read('baseline-hashes.json')
changed, missing = [], []
for entry in baseline:
    path = root / entry['path']
    if not path.exists():
        missing.append(entry['path'])
    elif hashlib.sha256(path.read_bytes()).hexdigest().upper() != entry['sha256'].upper():
        changed.append(entry['path'])
preserved = {'baseline_files': len(baseline), 'changed': changed, 'missing': missing}
write('preservation.json', preserved)

paths = []
for directory in ['Content/CombatFeedback/Audio', 'Content/CombatFeedback/Profiles/Audio',
                  'Source/JJK_theHiddenOnes/Training/Feedback/Audio']:
    paths.extend(p for p in (root / directory).rglob('*') if p.is_file())
paths.extend((root / 'Scripts/CombatFeedback').glob('D_*.py'))
paths.append(root / 'Docs/打击感开发/Agent_D_战斗音频.md')
manifest = [{'path': p.relative_to(root).as_posix(), 'bytes': p.stat().st_size,
             'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(set(paths))]
write('final-manifest.json', {'version': 'FB-D-20260930-v1', 'files': manifest})

pie = read('pie-report.json')
assets = read('asset-validation.json')
recording = read('recording-analysis.json')
cleanup = read('cleanup.json')
processes = read('handoff-processes.json')
checks = [
    ('Editor build succeeds', 'Result: Succeeded' in (out / 'build-final.log').read_text(encoding='utf-8-sig')),
    ('Cold asset validation passes', assets['status'] == 'passed'),
    ('Real PIE and local contracts pass', pie['status'] == 'passed'),
    ('Master capture has signal without clipping', recording['nonzero_samples'] > 0 and recording['clipped_samples'] == 0),
    ('Existing Content/Source/Config bytes preserved', not changed and not missing),
    ('Temporary settings restored and no dirty packages', cleanup['restored'] and not cleanup['dirty']),
    ('Editor/build/Cook/D regression released', not processes['active']),
]
for name in ['Docs/打击感开发/Agent_D_战斗音频.md', 'Content/CombatFeedback/Audio/Source/README.md']:
    data = (root / name).read_bytes()
    data.decode('utf-8')
    checks.append((name + ' UTF-8 without BOM', not data.startswith(b'\xef\xbb\xbf')))
summary = {'version': 'FB-D-20260930-v1',
           'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'status': 'passed' if all(passed for _, passed in checks) else 'failed',
           'checks': [{'name': name, 'passed': passed} for name, passed in checks],
           'pie_checks': len(pie['checks']), 'asset_checks': len(assets['checks']),
           'owned_files': len(manifest), 'preservation': preserved, 'recording': recording,
           'pending': ['A saves final FeedbackConsumerClasses binding',
                       'Human headphones/speaker listening and timbre approval',
                       'Same-version Cook/standalone verification by A/F']}
write('validation-summary.json', summary)
print(json.dumps({k: summary[k] for k in ['status', 'pie_checks', 'asset_checks', 'owned_files', 'pending']},
                 ensure_ascii=False, indent=2))
assert summary['status'] == 'passed', summary['checks']
