"""E source/asset preservation manifest. Run before editing, then with --verify.

python Scripts/CombatFeedback/E_snapshot.py [--verify]
Only writes Saved/FeedbackE; never modifies project sources or assets.
"""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/FeedbackE'
out.mkdir(parents=True, exist_ok=True)
owned = {'ArenaPlayerController.cpp', 'ArenaPlayerController.h',
         'ArenaCombatHudWidget.cpp', 'ArenaCombatHudWidget.h'}
files = list((root/'Source').rglob('*')) + list((root/'Config').rglob('*'))
for folder in ['Training', 'Characters/Ishigori/Repaired', 'UI/HUD/V9', 'CombatFeedback', 'Maps']:
    files += list((root/'Content'/folder).rglob('*.uasset'))
    files += list((root/'Content'/folder).rglob('*.umap'))
manifest = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in files if p.is_file() and p.name not in owned
            and '/Feedback/Camera/' not in p.as_posix()}
if '--verify' in sys.argv:
    before = json.loads((out/'preservation-baseline.json').read_text(encoding='utf-8'))
    changed = [p for p, h in before.items() if manifest.get(p) != h]
    result = {'passed': not changed, 'checked': len(before), 'changed': changed}
    (out/'preservation-result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result)); assert not changed
else:
    assert not (out/'preservation-baseline.json').exists(), 'Preserve original baseline'
    (out/'preservation-baseline.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    (out/'git-status-before.txt').write_bytes(subprocess.check_output(['git','status','--short'],cwd=root))
    for name in owned:
        source = root/'Source/JJK_theHiddenOnes/Training'/name
        (out/('baseline-'+name)).write_bytes(source.read_bytes())
    print('Captured', len(manifest), 'protected files and E source originals')
