"""E exclusive real PIE regression, with bounded local consumer contract checks.

python Scripts/CombatFeedback/E_run.py
Requires idle Editor on L_DojoArena. Saves runs and captures under Saved/FeedbackE.
Stops only the PIE it created and restores temporary settings even after failure.
"""
import datetime
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackE'
archive=out/'runs'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');archive.mkdir(parents=True)
(out/'latest-run.json').write_text(json.dumps({'path':str(archive)}),encoding='utf-8')
client=UnrealMCP();editor='EditorToolset.EditorAppToolset';owns=False;setup=False
case=sys.argv[1] if len(sys.argv)>1 else 'main'
assert case in ['main','edges','reentry']
script={'main':'E_pie.py','edges':'E_edges.py','reentry':'E_reentry.py'}[case]
def running():return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(name):
    result=run(root/'Scripts/CombatFeedback'/name)
    (archive/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
try:
    assert not running(),'Existing PIE belongs to another task'
    execute('E_setup.py');setup=True
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}});owns=True
    execute(script);deadline=time.monotonic()+300
    while time.monotonic()<deadline:
        try:report=json.loads((archive/'report.json').read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.3);continue
        if report['status']!='running':break
        time.sleep(.3)
    else:raise TimeoutError('E PIE suite timeout')
    print(json.dumps({'status':report['status'],'checks':len(report['checks']),
                     'failed':[x for x in report['checks'] if not x['passed']],
                     'error':report.get('error'),'archive':str(archive)},ensure_ascii=False,indent=2))
    assert report['status']=='passed'
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute('E_cleanup.py')
