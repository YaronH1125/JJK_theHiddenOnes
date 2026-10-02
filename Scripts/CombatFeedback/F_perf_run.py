"""Exclusive graphical PIE. Usage: python Scripts/CombatFeedback/F_perf_run.py [compare|cold-off]."""
from pathlib import Path
import datetime
import hashlib
import json
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_python import run
from ue_mcp import UnrealMCP
root=Path(__file__).resolve().parents[2]
compare='compare' in sys.argv
cold_off='cold-off' in sys.argv
optional='optional' in sys.argv
kind='optional' if optional else 'comparison' if compare else 'cold-off' if cold_off else 'performance'
out=root/'Saved/CombatFeedback/F'/kind/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True)
(root/'Saved/CombatFeedback/F'/('optional-pointer.json' if optional else 'compare-pointer.json' if compare else 'perf-pointer.json')).write_text(json.dumps({'path':str(out),'cold_off':cold_off}),encoding='utf-8')
meta={'candidate':'FA2-20260930-v2','editor_dll_sha256':hashlib.sha256((root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll').read_bytes()).hexdigest()}
(out/'meta.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
client=UnrealMCP();editor='EditorToolset.EditorAppToolset';owns=False;setup=False
def running():return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(path,name):
    result=run(path);(out/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
setupfile=out/'setup.py'
setupfile.write_text("from pathlib import Path\nexec(compile(Path("+repr(str(root/'Scripts/CombatFeedback/A2_setup.py'))+").read_text(encoding='utf-8'),'A2_setup','exec'))\nplay.set_editor_property('NewWindowWidth',1920)\nplay.set_editor_property('NewWindowHeight',1077)\n",encoding='utf-8')
try:
    assert not running(),'PIE already owned'
    execute(setupfile,'setup');setup=True
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}});owns=True
    execute(root/'Scripts/CombatFeedback'/('F_optional.py' if optional else 'F_compare.py' if compare else 'F_perf.py'),'launch')
    deadline=time.monotonic()+500
    while time.monotonic()<deadline:
        try:report=json.loads((out/'report.json').read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.5);continue
        if report['status']!='running':break
        time.sleep(.5)
    else:raise TimeoutError('F performance')
    print(json.dumps({'path':str(out),'status':report['status'],'error':report.get('error'),'comparison':report.get('comparison')},ensure_ascii=False,indent=2))
    assert report['status']==('passed' if compare or optional else 'sampled')
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute(root/'Scripts/CombatFeedback/A2_cleanup.py','cleanup')
