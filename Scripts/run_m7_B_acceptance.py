"""M7 B 类验收 runner；报告归档为 M7_B_Acceptance.json。"""
import json,time,hashlib
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 if running():mcp.tool(editor,'StopPIE')
 mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
 result=run(root/'Scripts/M7_B_acceptance.py');assert result['success'],result
 deadline=time.monotonic()+520
 while time.monotonic()<deadline:
  state=json.loads((root/'Saved/M7_B_acceptance.json').read_text(encoding='utf-8'))
  if state['status']!='running':break
  time.sleep(2)
 else:raise TimeoutError('M7B runner timeout')
 name='M7_B_Acceptance.json' if state['status']=='passed' else 'M7_B_Acceptance_failed_'+time.strftime('%H%M%S')+'.json'
 (root/'Docs/开发过程/验收记录'/name).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({k:v for k,v in state.items() if k!='checks'},ensure_ascii=False))
 print('checks',len(state['checks']),'failed',[c['name'] for c in state['checks'] if not c['passed']])
finally:
 if running():mcp.tool(editor,'StopPIE')
