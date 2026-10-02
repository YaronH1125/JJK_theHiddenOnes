"""Run E's final edges/main/reentry sequentially against one compiled binary.

python Scripts/CombatFeedback/E_validate_all.py (idle Editor on L_DojoArena)
Stops on a failed run; each E_run restores and releases its own PIE.
Writes Saved/FeedbackE/final-validation-runs.json, including the binary SHA-256.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackE'
binary=root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll'
state={'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'status':'running','runs':[]}
def write():(out/'final-validation-runs.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
write()
for case in ['edges','main','reentry']:
    with (out/('final-'+case+'-runner.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run([sys.executable,str(root/'Scripts/CombatFeedback/E_run.py'),case],cwd=root,stdout=log,stderr=subprocess.STDOUT)
    latest=Path(json.loads((out/'latest-run.json').read_text(encoding='utf-8'))['path'])
    report=json.loads((latest/'report.json').read_text(encoding='utf-8'))
    state['runs'].append({'case':case,'path':str(latest),'status':report['status'],'checks':len(report['checks']),'seconds':report.get('seconds')});write()
    if result.returncode or report['status']!='passed':
        state['status']='failed';write();raise RuntimeError('E final '+case+' failed; see '+str(latest))
assert hashlib.sha256(binary.read_bytes()).hexdigest()==state['binary_sha256']
state['status']='passed';write();print(json.dumps(state,indent=2))
