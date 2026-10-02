"""Run B's isolated PIE test and capture its paused real contact frames.

python Scripts/CombatFeedback/B_run.py
Requires idle Editor on L_DojoArena. Creates/stops one PIE, never saves assets.
Evidence is timestamped under Saved/FeedbackB/runs; latest-run.json points to it.
"""
from datetime import datetime
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run

root = Path(__file__).resolve().parents[2]
out = root/'Saved/FeedbackB/runs'/datetime.now().strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True)
(root/'Saved/FeedbackB/latest-run.json').write_text(json.dumps({'path':str(out)}),encoding='utf-8')
c=UnrealMCP(); editor='EditorToolset.EditorAppToolset'; owns=False
def running(): return json.loads(c.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    assert not running(), 'Existing PIE belongs to another operation'
    c.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}}); owns=True
    result=run(root/'Scripts/CombatFeedback/B_pie.py')
    (out/'launch.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
    deadline=time.monotonic()+300
    while time.monotonic()<deadline:
        report=json.loads((out/'report.json').read_text())
        if report['status']!='running': break
        time.sleep(.15)
    else: raise TimeoutError('B PIE timed out; StopPIE cleanup follows')
    print(json.dumps(dict(path=str(out),status=report['status'],checks=len(report['checks']),failed=[x for x in report['checks'] if not x['passed']],error=report.get('error')),indent=2))
    assert report['status']=='passed'
finally:
    if owns and running():c.tool(editor,'StopPIE')
    if owns:
        cleanup=run(root/'Scripts/CombatFeedback/B_cleanup.py')
        (out/'cleanup.json').write_text(json.dumps(cleanup,ensure_ascii=False,indent=2),encoding='utf-8')
        assert cleanup['success'],cleanup
