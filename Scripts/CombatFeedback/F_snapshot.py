"""F provenance/integrity audit. Usage: python Scripts/CombatFeedback/F_snapshot.py [--verify].

Checks A's frozen candidate without restoring it. New F evidence is excluded from
the protected file set. Never changes source/config/assets or stages Git files.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from datetime import datetime

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/CombatFeedback/F'
OUT.mkdir(parents=True, exist_ok=True)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def current():
    return {p.relative_to(ROOT).as_posix(): sha(p)
            for folder in ('Source', 'Config', 'Content')
            for p in sorted((ROOT / folder).rglob('*')) if p.is_file()}

def differences(expected):
    return [{'path': name, 'expected': digest,
             'actual': sha(ROOT/name) if (ROOT/name).is_file() else None}
            for name, digest in expected.items()
            if not (ROOT/name).is_file() or sha(ROOT/name) != digest]

candidate = json.loads((ROOT/'Saved/FeedbackA2/candidate/manifest.json').read_text(encoding='utf-8'))
protected = {k:v for k,v in candidate['files'].items() if k.startswith(('Source/', 'Config/', 'Content/'))}
report = {'candidate': 'FA2-20260930-v2', 'time': datetime.now().astimezone().isoformat(),
          'candidate_protected_count': len(protected), 'candidate_differences': differences(protected),
          'candidate_existing_script_differences': differences({k:v for k,v in candidate['files'].items() if k.startswith('Scripts/')}),
          'head': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
          'editor_dll_sha256': sha(ROOT/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll'),
          'expected_editor_dll_sha256': candidate.get('editor_dll_sha256')}
if '--verify' in sys.argv:
    baseline=json.loads((OUT/'baseline.json').read_text(encoding='utf-8'))
    report['f_baseline_differences']=differences(baseline['files'])
    report['added_protected_files']=sorted(set(current())-set(baseline['files']))
    path=OUT/('preservation-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
else:
    path=OUT/'baseline.json'
    assert not path.exists(), 'Preserve the initial F snapshot; use --verify instead'
    report['files']=current()
    (OUT/'git-status-before.txt').write_bytes(subprocess.check_output(['git','status','--short'],cwd=ROOT))
path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='files'},ensure_ascii=False,indent=2))
assert not report['candidate_differences'], 'Candidate source/config/assets changed before/during F'
assert not report['candidate_existing_script_differences'], 'Existing candidate test scripts changed'
assert report['editor_dll_sha256']==report['expected_editor_dll_sha256'], 'Editor DLL differs from A candidate'
assert not report.get('f_baseline_differences'), 'Protected files changed during F'
