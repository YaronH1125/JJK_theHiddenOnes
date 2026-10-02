"""Exclusive R1 PIE runner using saved production wiring.
Run: python Scripts/CombatFeedback/R1_run.py
Archives fresh results in Saved/FeedbackRevisionR1/runs, preserves A2 setup evidence.
"""
import datetime
import hashlib
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[2]
out=root/'Saved/FeedbackRevisionR1'
archive=out/'runs'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
archive.mkdir(parents=True)
(out/'latest-run.json').write_text(json.dumps({'path':str(archive),'capture':'--capture' in sys.argv,
                                             'visual_only':'--visual-only' in sys.argv}),encoding='utf-8')
old_setup=root/'Saved/FeedbackA2/setup.json'
saved_setup=old_setup.read_bytes() if old_setup.exists() else None
client=UnrealMCP(); editor='EditorToolset.EditorAppToolset'; owns=False; setup=False
def running():return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(name):
    result=run(root/'Scripts/CombatFeedback'/name)
    (archive/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
try:
    assert not running(),'Existing PIE must be preserved'
    execute('A2_setup.py');setup=True
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}});owns=True
    execute('R1_pie.py');deadline=time.monotonic()+270
    while time.monotonic()<deadline:
        try:report=json.loads((archive/'report.json').read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.3);continue
        if report['status']!='running':break
        time.sleep(.3)
    else:raise TimeoutError('R1 runner')
    report['editor_dll_sha256']=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll').read_bytes()).hexdigest()
    (archive/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'archive':str(archive),'status':report['status'],'checks':len(report['checks']),
                      'failed':[x for x in report['checks'] if not x['passed']],'error':report.get('error')},ensure_ascii=False,indent=2))
    assert report['status']=='passed'
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute('A2_cleanup.py')
    if saved_setup is not None:old_setup.write_bytes(saved_setup)
