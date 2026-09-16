"""M4 changes: rerun M3 rules and M2 lifecycle regression in a fresh PIE; preserve every failed report."""
import json,time,hashlib,sys
stage=sys.argv[1] if len(sys.argv)>1 else "M3"
assert stage in ("M3","M4")
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1]
mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():mcp.tool(editor,'StopPIE')
    mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
    result=run(root/f'Scripts/{stage}_acceptance.py');assert result['success'],result
    deadline=time.monotonic()+520
    while time.monotonic()<deadline:
        try: state=json.loads((root/f'Saved/{stage}_acceptance.json').read_text(encoding='utf-8'))
        except json.JSONDecodeError:
            time.sleep(.1);continue
        if state['status']!='running':break
        time.sleep(1)
    else:raise TimeoutError('M3 runner timeout')
    state['snapshot']={str(p.relative_to(root)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
        for folder,pattern in [('Source','*.h'),('Source','*.cpp'),('Content/Training','*.uasset'),('Scripts',stage+'*.py'),('Config','*.ini'),('Content/Maps','L_TrainingArena.umap')]
        for p in (root/folder).rglob(pattern)}
    path=root/'Docs/开发过程/验收记录'/(f'M5_{stage}_Regression.json' if state['status']=='passed' else f'M5_{stage}_Regression_failed_'+time.strftime('%H%M%S')+'.json')
    path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in state.items() if k not in ('snapshot','checks')},ensure_ascii=False))
    print('checks:',len(state['checks']),'failed:',[c for c in state['checks'] if not c['passed']])
    assert state['status']=='passed' and len(state['checks'])==(118 if stage=='M3' else 105) and all(c['passed'] for c in state['checks'])
finally:
    if running():mcp.tool(editor,'StopPIE')
