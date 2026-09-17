"""M7 A 类验收 runner；报告独立归档为 M7_A_Acceptance.json。"""
import json,time,hashlib
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 if running():mcp.tool(editor,'StopPIE')
 mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
 result=run(root/'Scripts/M7_A_acceptance.py');assert result['success'],result
 deadline=time.monotonic()+300
 while time.monotonic()<deadline:
  state=json.loads((root/'Saved/M7_A_acceptance.json').read_text(encoding='utf-8'))
  if state['status']!='running':break
  time.sleep(1)
 else:raise TimeoutError('M7A runner timeout')
 name='M7_A_Acceptance.json' if state['status']=='passed' else 'M7_A_Acceptance_failed_'+time.strftime('%H%M%S')+'.json'
 (root/'Docs/开发过程/验收记录'/name).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({k:v for k,v in state.items() if k not in ('checks',)},ensure_ascii=False))
 print('checks',len(state['checks']),'failed',[c['name'] for c in state['checks'] if not c['passed']])
finally:
 if running():mcp.tool(editor,'StopPIE')
