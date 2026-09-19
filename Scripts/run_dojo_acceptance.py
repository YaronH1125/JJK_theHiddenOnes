"""Run a fresh Dojo PIE and preserve every outcome in a timestamped evidence file."""
import json,time,sys,hashlib
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1]
mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
(root/'Docs/开发过程/验收记录').mkdir(parents=True,exist_ok=True)

def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():mcp.tool(editor,'StopPIE')
    mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}})
    r=run(root/'Scripts/Dojo_acceptance.py');assert r['success'],r
    deadline=time.monotonic()+1100
    while time.monotonic()<deadline:
        state=json.loads((root/'Saved/Dojo_acceptance.json').read_text(encoding='utf-8'))
        if state['status']!='running':break
        time.sleep(2)
    else:raise TimeoutError('DOJO runner timeout')
    state['snapshot']={p.relative_to(root).as_posix():hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for folder,pattern in [('Source','*.cpp'),('Source','*.h'),('Config','*.ini'),('Content/Training','*.uasset'),('Content/Maps','*.umap')] for p in (root/folder).rglob(pattern)}
    out=root/'Docs/开发过程/验收记录'/('Dojo_acceptance_'+time.strftime('%Y%m%d_%H%M%S')+'.json')
    out.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(state,ensure_ascii=False));assert state['status']=='passed'
finally:
    if running():mcp.tool(editor,'StopPIE')
