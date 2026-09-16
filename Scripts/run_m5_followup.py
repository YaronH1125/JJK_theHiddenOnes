import sys,json,time,hashlib
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[1];stage=sys.argv[1] if len(sys.argv)>1 else 'UI'
script,report,expected={'UI':('M5_followup.py','M5_followup.json',43),'M3':('M3_acceptance.py','M3_acceptance.json',118),'M4':('M4_acceptance.py','M4_acceptance.json',105),'M5':('M5_acceptance.py','M5_acceptance.json',68)}[stage]
m=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running():return json.loads(m.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
 if running():m.tool(editor,'StopPIE')
 m.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
 result=run(root/'Scripts'/script);assert result['success'],result
 deadline=time.monotonic()+520
 while time.monotonic()<deadline:
  try:state=json.loads((root/'Saved'/report).read_text(encoding='utf-8'))
  except (OSError,json.JSONDecodeError):
   time.sleep(.1);continue
  if state['status']!='running':break
  time.sleep(1)
 else:raise TimeoutError(stage)
 state['snapshot']={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for folder,pat in [('Source','*.h'),('Source','*.cpp'),('Config','*.ini'),('Content/Training','*.uasset'),('Scripts',script)] for p in (root/folder).rglob(pat)}
 suffix='' if state['status']=='passed' else '_failed_'+time.strftime('%H%M%S')
 (root/'Docs/开发过程/验收记录'/('M5_Followup_'+stage+suffix+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({k:v for k,v in state.items() if k not in ('snapshot','checks')},ensure_ascii=False))
 print('checks',len(state['checks']),'failed',[x for x in state['checks'] if not x['passed']])
 assert state['status']=='passed' and all(x['passed'] for x in state['checks']) and (expected is None or len(state['checks'])==expected)
finally:
 if running():m.tool(editor,'StopPIE')
