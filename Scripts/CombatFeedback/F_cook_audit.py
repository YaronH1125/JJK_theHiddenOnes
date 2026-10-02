"""F container/hard-reference audit. Run after F_build and UnrealPak -ListContainer.

Usage: python Scripts/CombatFeedback/F_cook_audit.py. UnrealPak CSV PackageName
may be empty; derive /Game names from mounted uasset/umap Filename, not empties.
"""
import csv
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/CombatFeedback/F'
PACKAGE=ROOT/'Saved/Packages/FeedbackF_FA2_v2/Windows'
rows=list(csv.DictReader((OUT/'package-contents.csv').open(encoding='utf-8'),skipinitialspace=True))
present=set()
for row in rows:
    path=row['Filename']
    if '/JJK_theHiddenOnes/Content/' in path and path.endswith(('.uasset','.umap')):
        present.add('/Game/'+path.split('/JJK_theHiddenOnes/Content/')[1].rsplit('.',1)[0])
audit=json.loads((ROOT/'Saved/FeedbackA2/dependency-audit.json').read_text(encoding='utf-8'))
expected={x for x in audit['graph'] if x.startswith('/Game/')}
missing=sorted(expected-present)
integrity={p.relative_to(PACKAGE).as_posix():hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
           for p in PACKAGE.rglob('*') if p.is_file() and (p.suffix in ['.exe','.pak','.utoc','.ucas'] or p.name=='AssetRegistry.bin')}
report={'candidate':'FA2-20260930-v2','hard_dependencies_expected':len(expected),'present':len(expected)-len(missing),
        'missing':missing,'feedback_assets':sorted(x for x in present if '/CombatFeedback/' in x or '/Repaired/Feedback/' in x),
        'maps':[x for x in ['/Game/Maps/L_DojoArena','/Game/Maps/L_TrainingArena'] if x in present],
        'package_path':str(PACKAGE),'exe':str(PACKAGE/'JJK_theHiddenOnes.exe'),
        'all_directory_bytes':sum(p.stat().st_size for p in PACKAGE.rglob('*') if p.is_file()),'hashes':integrity,
        'note':'Cook/container presence is separate from runtime/human sound and visual checks'}
(OUT/'cook-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['hashes','feedback_assets']},ensure_ascii=False,indent=2))
assert not missing,'Required A hard dependency absent from final container'
