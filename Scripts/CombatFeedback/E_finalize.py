"""After E releases Editor: preservation check, source hashes and evidence summary.

python Scripts/CombatFeedback/E_finalize.py
Only writes Saved/FeedbackE; no assets, source/config edits, staging or commits.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
from datetime import datetime, timezone, timedelta

root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackE'
subprocess.run(['python',str(root/'Scripts/CombatFeedback/E_snapshot.py'),'--verify'],cwd=root,check=True)
files=list((root/'Source/JJK_theHiddenOnes/Training/Feedback/Camera').glob('*'))+list((root/'Scripts/CombatFeedback').glob('E_*.py'))
files += [root/'Source/JJK_theHiddenOnes/Training'/name for name in ['ArenaPlayerController.cpp','ArenaPlayerController.h','ArenaCombatHudWidget.cpp','ArenaCombatHudWidget.h']]
files += [root/'Docs/打击感开发/Agent_E_镜头与HUD.md',root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll']
manifest={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}
final_runs=json.loads((out/'final-validation-runs.json').read_text(encoding='utf-8'))
assert final_runs['status']=='passed' and len(final_runs['runs'])==3
assert final_runs['binary_sha256']==manifest['Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll']
(out/'final-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
reports=[]
for p in sorted((out/'runs').glob('*/report.json')):
    r=json.loads(p.read_text(encoding='utf-8'))
    reports.append({'path':p.relative_to(root).as_posix(),'status':r['status'],'checks':len(r['checks']),
                    'failures':[x['name'] for x in r['checks'] if not x['passed']],
                    'error':r.get('error'),'seconds':r.get('seconds')})
# The Windows Python launcher can remain as this finalizer's parent until it exits.
# Exclude only the two exact finalizer PIDs, not other Python workers.
finalizer_pids=[os.getpid(),os.getppid()]
proc_command="@(Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool,AutomationTool,python -ErrorAction SilentlyContinue | Where-Object { $_.Id -notin @("+','.join(map(str,finalizer_pids))+") } | Select-Object Id,ProcessName) | ConvertTo-Json -Compress"
proc=subprocess.check_output(['powershell','-NoProfile','-Command',proc_command],text=True).strip()
active=json.loads(proc) if proc else []
assert not active,active
(out/'handoff-processes.json').write_text(json.dumps({'active':active,'finalizer_pids_excluded':finalizer_pids,'PIE':'stopped by E_run','Cook':'not started'},indent=2),encoding='utf-8')
summary={'finalized_at':datetime.now(timezone(timedelta(hours=8))).isoformat(timespec='seconds'),
         'scope':'Agent E only, A interface v1; final A integration and F packaging pending',
         'build':'Saved/FeedbackE/build.log','preservation':json.loads((out/'preservation-result.json').read_text()),
         'reports':reports,'processes':active,'human_comfort':'pending user/F',
         'final_same_binary':final_runs,
         'hitstop':'default remains off; A FB-T17 120 FPS gate not revalidated by E'}
(out/'validation-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
