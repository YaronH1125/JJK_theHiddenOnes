"""Fresh F-owned replay index. Usage: python Scripts/CombatFeedback/F_regress.py.

Runs A's production-binding harnesses serially, copies fresh reports/screens/audio
to F evidence, and keeps failures. No old pass is counted as a new F run.
"""
from pathlib import Path
import datetime
import json
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/CombatFeedback/F/regressions'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
OUT.mkdir(parents=True)
cases=[('run','integrated'),('run','extended'),('run','shift'),('run','loop'),
       ('replay','a-edge'),('replay','c-main'),('replay','c-extra'),('replay','d-main'),
       ('replay','e-edges'),('replay','e-main'),('replay','e-reentry'),
       ('replay','a-reentry'),('replay','a-reentry'),('replay','a-reentry'),
       ('replay','ig20'),('replay','ig21'),('replay','a-clock')]
index={'candidate':'FA2-20260930-v2','status':'running','runs':[],
       'human_play':'not performed','human_listening':'not performed',
       'method':'fresh graphical PIE, shared/native simulated input and labelled CONTRACT fixtures'}
for i,(kind,case) in enumerate(cases,1):
    prefix=f'{i:02d}-{case}'
    destination=OUT/prefix
    log=OUT/(prefix+'.log')
    parent=ROOT/'Saved/FeedbackA2'/('runs' if kind=='run' else 'replays')
    before=set(parent.iterdir())
    started=time.time()
    print('START '+prefix,flush=True)
    with log.open('w',encoding='utf-8') as stream:
        process=subprocess.run([sys.executable,str(ROOT/'Scripts/CombatFeedback'/('A2_run.py' if kind=='run' else 'A2_replay.py')),case],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
    fresh=sorted(set(parent.iterdir())-before)
    assert len(fresh)==1, (case,fresh)
    shutil.copytree(fresh[0],destination)
    path=destination/'report.json'
    report=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
    checks=report.get('checks',[])
    row={'case':case,'method':kind,'fresh_source':str(fresh[0]),'evidence':str(destination),
         'exit_code':process.returncode,'status':report.get('status','harness failed'),
         'checks':len(checks),'failed':[c for c in checks if not c.get('passed')],
         'contract_or_synthetic':[c['name'] for c in checks if any(s in c['name'].lower() for s in ['contract','synthetic'])],
         'seconds':time.time()-started,'error':report.get('error')}
    index['runs'].append(row)
    (OUT/'index.json').write_text(json.dumps(index,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in row.items() if k not in ('failed','contract_or_synthetic')},ensure_ascii=False),flush=True)
    if row['error'] or row['status']=='harness failed':
        print('STOP after harness error; inspect cleanup before continuing',flush=True)
        break
index['status']='executed with failures' if any(x['exit_code'] for x in index['runs']) else 'passed'
(OUT/'index.json').write_text(json.dumps(index,ensure_ascii=False,indent=2),encoding='utf-8')
print('F regression index '+str(OUT/'index.json'),flush=True)
