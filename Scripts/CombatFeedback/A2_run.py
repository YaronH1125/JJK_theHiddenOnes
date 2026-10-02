"""Run actual integrated combat with saved production assets, then release PIE."""
import datetime
import hashlib
import json
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[2]; out=root/'Saved/FeedbackA2'
case=sys.argv[1] if len(sys.argv)>1 else 'integrated'
scripts={'integrated':'A2_pie.py','extended':'A2_extended.py','loop':'A2_loop.py','shift':'A2_shift.py'}
assert case in scripts
archive=out/'runs'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S'); archive.mkdir(parents=True)
meta={'case':case,
      'binary_sha256':hashlib.sha256((root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll').read_bytes()).hexdigest(),
      'fighter_asset_sha256':hashlib.sha256((root/'Content/Training/DA_Fighter_Ishigori.uasset').read_bytes()).hexdigest()}
(archive/'meta.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
(out/'latest-run.json').write_text(json.dumps({'path':str(archive)}), encoding='utf-8')
client=UnrealMCP(); editor='EditorToolset.EditorAppToolset'; owns=False; setup=False
def running():return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(name):
    result=run(root/'Scripts/CombatFeedback'/name)
    (archive/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
try:
    assert not running(),'An existing PIE owns the editor'
    execute('A2_setup.py'); setup=True
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}}); owns=True
    execute(scripts[case]); deadline=time.monotonic()+300
    while time.monotonic()<deadline:
        try: report=json.loads((archive/'report.json').read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.3);continue
        if report['status']!='running':break
        time.sleep(.3)
    else:raise TimeoutError('A2 suite timed out')
    print(json.dumps({'archive':str(archive),'status':report['status'],'checks':len(report['checks']),
                     'failed':[x for x in report['checks'] if not x['passed']],'error':report.get('error')},ensure_ascii=False,indent=2))
    assert report['status']=='passed'
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute('A2_cleanup.py')
