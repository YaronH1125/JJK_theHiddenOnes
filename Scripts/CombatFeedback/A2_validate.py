"""Sequential regression replay; one Editor/PIE owner, one unchanged binary."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackA2'
binary=root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll'
state={'status':'running','binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'runs':[]}
def write():(out/'validation-runs.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
write()
cases=sys.argv[1:] or ['c-extra','d-main','e-edges','e-main','e-reentry','a-reentry','a-reentry','a-reentry','ig20','ig21','a-clock']
for i,case in enumerate(cases):
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==state['binary_sha256'],'Binary changed during acceptance'
    log=out/(str(i+1).zfill(2)+'-'+case+'.log')
    with log.open('w',encoding='utf-8') as f:
        result=subprocess.run([sys.executable,'-X','utf8',str(root/'Scripts/CombatFeedback/A2_replay.py'),case],cwd=root,stdout=f,stderr=subprocess.STDOUT)
    path=sorted((out/'replays').glob('*-'+case))[-1]
    meta=json.loads((path/'meta.json').read_text(encoding='utf-8'))
    state['runs'].append({'case':case,'path':str(path),'exit_code':result.returncode,**meta});write()
    print(json.dumps({'case':case,'status':meta['status'],'checks':meta.get('checks'),'failed':meta.get('failed')},ensure_ascii=False),flush=True)
    cleanup=path/'cleanup.json'
    if not cleanup.exists() or not json.loads(cleanup.read_text(encoding='utf-8'))['success']:
        state['status']='cleanup failed';write();raise RuntimeError('Review '+str(log))
state['status']='passed' if all(x['exit_code']==0 for x in state['runs']) else 'has failures'
write()
