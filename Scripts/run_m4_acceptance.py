"""Run M4 rules and M2 lifecycle regression in a fresh PIE; preserve every failed report."""
import json,time,hashlib
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1]
mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():mcp.tool(editor,'StopPIE')
    mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
    result=run(root/'Scripts/M4_acceptance.py');assert result['success'],result
    deadline=time.monotonic()+520
    while time.monotonic()<deadline:
        state=json.loads((root/'Saved/M4_acceptance.json').read_text(encoding='utf-8'))
        if state['status']!='running':break
        time.sleep(1)
    else:raise TimeoutError('M4 runner timeout')
    state['snapshot']={str(p.relative_to(root)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
        for folder,pattern in [('Source','*.h'),('Source','*.cpp'),('Content/Training','*.uasset'),('Scripts','M4*.py')]
        for p in (root/folder).rglob(pattern)}
    path=root/'Docs/开发过程/验收记录'/('M4_Acceptance.json' if state['status']=='passed' else 'M4_Acceptance_failed_'+time.strftime('%H%M%S')+'.json')
    path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in state.items() if k not in ('snapshot','checks')},ensure_ascii=False))
    print('checks:',len(state['checks']),'failed:',[c for c in state['checks'] if not c['passed']])
    assert state['status']=='passed' and len(state['checks'])>=105 and all(c['passed'] for c in state['checks'])
finally:
    if running():mcp.tool(editor,'StopPIE')
