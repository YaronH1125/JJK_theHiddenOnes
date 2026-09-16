import json,time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 if running():mcp.tool(editor,'StopPIE')
 mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
 result=run(root/'Scripts/M3_demo.py');assert result['success'],result
 deadline=time.monotonic()+170
 while time.monotonic()<deadline:
  state=json.loads((root/'Saved/M3_demo.json').read_text(encoding='utf-8'))
  if state['status']!='running':break
  time.sleep(1)
 else:raise TimeoutError('demo runner')
 (root/'Docs/开发过程/验收记录/M3_Demo.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 print('Status:',state['status'],'checks:',len(state['checks']))
 print('Failures:',[c for c in state['checks'] if not c['passed']]);print(state.get('error',''))
 assert state['status']=='passed'
finally:
 if running():mcp.tool(editor,'StopPIE')
