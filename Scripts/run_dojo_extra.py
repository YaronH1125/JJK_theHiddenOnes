import json,time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];c=UnrealMCP();e='EditorToolset.EditorAppToolset'
(root/'Docs/开发过程/验收记录').mkdir(parents=True,exist_ok=True)

def running():return json.loads(c.tool(e,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():c.tool(e,'StopPIE')
    c.tool(e,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':2}})
    r=run(root/'Scripts/Dojo_extra.py');assert r['success'],r
    deadline=time.monotonic()+90
    while time.monotonic()<deadline:
        state=json.loads((root/'Saved/Dojo_extra.json').read_text(encoding='utf-8'))
        if state['status']!='running':break
        time.sleep(1)
    else:raise TimeoutError('extra checks timeout')
    (root/'Docs/开发过程/验收记录'/('Dojo_extra_'+time.strftime('%Y%m%d_%H%M%S')+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(state,ensure_ascii=False));assert state['status']=='passed'
finally:
    if running():c.tool(e,'StopPIE')
