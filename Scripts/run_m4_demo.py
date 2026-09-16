"""M4 graphic demo + actual UMG screenshot, preserve failures separately."""
import json,time,sys
sys.stdout.reconfigure(encoding="utf-8")
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];mcp=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(mcp.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 if running():mcp.tool(editor,'StopPIE')
 mcp.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
 result=run(root/'Scripts/M4_demo.py');assert result['success'],result
 deadline=time.monotonic()+180
 while time.monotonic()<deadline:
  state=json.loads((root/'Saved/M4_demo.json').read_text(encoding='utf-8'))
  if state['status']!='running':break
  time.sleep(1)
 else:raise TimeoutError('M4 demo runner')
 if state['status']=='passed':
  import struct
  screenshot=root/'Docs/开发过程/验收记录/M4_训练面板.png'
  assert screenshot.is_file() and screenshot.stat().st_mtime>=state['started_at']
  state['screenshot_size']=list(struct.unpack('>II',screenshot.read_bytes()[16:24]))
 path=root/'Docs/开发过程/验收记录'/('M4_Demo.json' if state['status']=='passed' else 'M4_Demo_failed_'+time.strftime('%H%M%S')+'.json')
 path.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps(state,ensure_ascii=False));assert state['status']=='passed'
finally:
 if running():mcp.tool(editor,'StopPIE')
