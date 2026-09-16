import json,time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset';results=[]
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 for cycle in range(3):
  if running():mcp.tool(editor,'StopPIE')
  mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
  r=run(root/'Scripts/M5_reentry.py');assert r['success'],r
  deadline=time.monotonic()+60
  while time.monotonic()<deadline:
   s=json.loads((root/'Saved/M5_reentry.json').read_text(encoding='utf-8'))
   if s['status']!='running':break
   time.sleep(.5)
  results.append(s);assert s['status']=='passed' and len(s['checks'])==7 and all(c['passed'] for c in s['checks']),s
  mcp.tool(editor,'StopPIE')
 (root/'Docs/开发过程/验收记录/M5_Reentry.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
 print('3 fresh PIE entries, 21 checks passed')
finally:
 if running():mcp.tool(editor,'StopPIE')
