"""Exclusive real PIE audio regression and raw Master mixer recording.

python Scripts/CombatFeedback/D_run.py (idle Editor on L_DojoArena)
Never saves shared assets; stops PIE and restores binding/cvars on every exit.
Archives reports/launch logs to Saved/FeedbackD/runs/<timestamp>/.
"""
import datetime
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[2]
out=root/'Saved/FeedbackD'
archive=out/'runs'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
archive.mkdir(parents=True,exist_ok=True)
client=UnrealMCP();editor='EditorToolset.EditorAppToolset'
def running(): return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(name):
    result=run(root/'Scripts/CombatFeedback'/name)
    (archive/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
    return result
owns=False;setup=False
case=sys.argv[1] if len(sys.argv)>1 else 'main'
assert case in ['main','smoke','contracts']
script={'main':'D_pie.py','smoke':'D_smoke.py','contracts':'D_contract.py'}[case]
report_name={'main':'pie-report.json','smoke':'smoke-report.json','contracts':'contract-report.json'}[case]
wav_name={'main':'D-real-combat.wav','smoke':'D-smoke.wav','contracts':None}[case]
try:
    assert not running(),'Another task owns PIE'
    execute('D_setup.py');setup=True
    if (out/report_name).exists():
        (archive/'prior-report.json').write_bytes((out/report_name).read_bytes())
        (out/report_name).unlink()
    if wav_name and (out/wav_name).exists():(archive/'prior-recording.wav').write_bytes((out/wav_name).read_bytes())
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}});owns=True
    execute(script)
    deadline=time.monotonic()+360
    while time.monotonic()<deadline:
        try: report=json.loads((out/report_name).read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.5);continue
        if report['status']!='running':break
        time.sleep(.5)
    else:raise TimeoutError('D PIE suite timeout')
    (archive/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if wav_name and (out/wav_name).exists():(archive/'recording.wav').write_bytes((out/wav_name).read_bytes())
    print(json.dumps({'status':report['status'],'checks':len(report['checks']),
                      'failed':[x for x in report['checks'] if not x['passed']],
                      'error':report.get('error'),'archive':str(archive)},ensure_ascii=False,indent=2))
    assert report['status']=='passed'
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute('D_cleanup.py')
