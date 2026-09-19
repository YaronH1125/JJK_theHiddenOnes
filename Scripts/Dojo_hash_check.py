"""Verify immutable source art against the versioned import baseline (SHA-256 file manifest)."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
baseline=root/'Scripts/Baselines/external_assets.json'
expected={row['path']:row['sha256'] for row in json.loads(baseline.read_text(encoding='utf-8'))['packages']['Mishima_DOJO']['files']}
actual={p.relative_to(root).as_posix():hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for p in (root/'Content/Mishima_DOJO').rglob('*') if p.is_file()}
result={'files':len(actual),'missing':sorted(expected.keys()-actual.keys()),'added':sorted(actual.keys()-expected.keys()),'changed':[p for p in expected.keys()&actual.keys() if expected[p]!=actual[p]]}
result['passed']=not any(result[k] for k in ('missing','added','changed'))
(root/'Saved').mkdir(parents=True,exist_ok=True)
(root/'Saved/Dojo_source_integrity.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result));assert result['passed']
