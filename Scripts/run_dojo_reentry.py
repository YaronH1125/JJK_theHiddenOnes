import json,time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];c=UnrealMCP();e='EditorToolset.EditorAppToolset';results=[]
(root/'Docs/开发过程/验收记录').mkdir(parents=True,exist_ok=True)

def running():return json.loads(c.tool(e,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():c.tool(e,'StopPIE')
    for i in range(3):
        c.tool(e,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':3}})
        r=run(root/'Scripts/Dojo_reentry.py');results.append(r);assert r['success'],r
        time.sleep(2);c.tool(e,'StopPIE')
finally:
    if running():c.tool(e,'StopPIE')
    (root/'Docs/开发过程/验收记录'/('Dojo_reentry_'+time.strftime('%Y%m%d_%H%M%S')+'.json')).write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print('3 fresh PIE sessions passed')
