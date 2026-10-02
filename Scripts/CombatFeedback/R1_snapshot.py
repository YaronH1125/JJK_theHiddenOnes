"""Freeze / verify R1 build inputs and protect the human-feedback baseline.

Usage: python Scripts/CombatFeedback/R1_snapshot.py --freeze|--verify
Never restores files or stages Git. Rejects overwriting a prior frozen candidate.
"""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = root/'Saved/FeedbackRevisionR1'
def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
def inventory(folders):
    return {p.relative_to(root).as_posix(): sha(p)
            for folder in folders for p in sorted((root/folder).rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.pyo')}
baseline = json.loads((out/'baseline-manifest.json').read_text(encoding='utf-8'))
changed = [name for name, digest in baseline.items()
           if not (root/name).is_file() or sha(root/name) != digest]
protected = [name for name in baseline if name.startswith((
    'Content/Mishima_DOJO/', 'Content/EnergyBeam/', 'Content/GoodParticleBeamAndRay/',
    'Content/CombatFeedback/Audio/', 'Content/CombatFeedback/Profiles/Audio/'))]
protected_changes = [name for name in changed if name in protected]
report = {'candidate': 'FR1-20261001', 'time': datetime.datetime.now().astimezone().isoformat(),
          'baseline_files': len(baseline), 'changed_from_baseline': changed,
          'protected_files': len(protected), 'protected_changes': protected_changes}
assert not protected_changes, 'Vendor/audio preservation failed: '+str(protected_changes)
path = out/'candidate/manifest.json'
if '--freeze' in sys.argv:
    assert not path.exists(), 'Preserve existing freeze; create a distinct revision'
    report['head'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    report['files'] = inventory(['Source','Config','Content','Scripts'])
    report['editor_dll_sha256'] = sha(root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
elif '--verify' in sys.argv:
    frozen = json.loads(path.read_text(encoding='utf-8'))
    current = inventory(['Source','Config','Content','Scripts'])
    report['freeze_differences'] = [name for name,digest in frozen['files'].items() if current.get(name)!=digest]
    report['freeze_additions'] = sorted(set(current)-set(frozen['files']))
    report['editor_binary_unchanged'] = sha(root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll')==frozen['editor_dll_sha256']
    target = out/('preservation-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    assert not report['freeze_differences'] and not report['freeze_additions'] and report['editor_binary_unchanged'],report
else:
    raise SystemExit('Choose --freeze or --verify')
print(json.dumps({k:v for k,v in report.items() if k!='files'},ensure_ascii=False,indent=2))
