"""Run an existing regression on the SAVED integrated binding. Never call its isolated setup.

Uses A2 setup/cleanup, preserves prior evidence, corrects the old positional
Rotator fixture in memory, and labels the exact source and binary in each run.
"""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time
import traceback
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run
root=Path(__file__).resolve().parents[2];out=root/'Saved/FeedbackA2'
cases={
 'a-clock':('Scripts/feedback_a_regression.py','Saved/FeedbackA/regression.json',480),
 'a-edge':('Scripts/feedback_a_edge_cases.py','Saved/FeedbackA/edge_cases.json',240),
 'a-reentry':('Scripts/feedback_a_reentry.py','Saved/FeedbackA/reentry.json',100),
 'c-main':('Scripts/CombatFeedback/C_pie.py','Saved/FeedbackC/pie-report.json',180),
 'c-extra':('Scripts/CombatFeedback/C_extra_pie.py','Saved/FeedbackC/extra-report.json',180),
 'd-main':('Scripts/CombatFeedback/D_pie.py','Saved/FeedbackD/pie-report.json',360),
 'e-main':('Scripts/CombatFeedback/E_pie.py',None,300),
 'e-edges':('Scripts/CombatFeedback/E_edges.py',None,150),
 'e-reentry':('Scripts/CombatFeedback/E_reentry.py',None,150),
 'ig20':('Scripts/ig20_acceptance.py','Saved/IG20/acceptance.json',240),
 'ig21':('Scripts/ig21_acceptance.py','Saved/IG21/acceptance.json',240),
}
case=sys.argv[1];src,report_rel,timeout=cases[case]
archive=out/'replays'/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+case);archive.mkdir(parents=True)
report_path=root/report_rel if report_rel else archive/'report.json'
if report_path.exists():shutil.copy2(report_path,archive/'prior-report.json');report_path.unlink()
meta={'case':case,'source':src,'source_sha256':hashlib.sha256((root/src).read_bytes()).hexdigest(),
      'binary_sha256':hashlib.sha256((root/'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll').read_bytes()).hexdigest(),
      'fixture':'A2 production binding from disk; old positional Rotator corrected in memory', 'status':'running'}
(archive/'meta.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
# Existing E scripts use an archive pointer. Restore it after this exclusive run.
e_pointer=root/'Saved/FeedbackE/latest-run.json';old_pointer=e_pointer.read_bytes() if e_pointer.exists() else None
if case.startswith('e-'):e_pointer.write_text(json.dumps({'path':str(archive)}),encoding='utf-8')
wrapper=archive/'launch.py'
wrapper_body = '''
text=Path(__file__).read_text(encoding='utf-8')
text=text.replace('unreal.Rotator(0,yaw,0)', 'unreal.Rotator(pitch=0,yaw=yaw,roll=0)')
text=text.replace('unreal.Rotator(0, yaw, 0)', 'unreal.Rotator(pitch=0,yaw=yaw,roll=0)')
exec(compile(text,__file__,'exec'))
for fighter in [gm.get_player_fighter(),gm.get_opponent_fighter()]:
    check('A2 '+fighter.get_name()+' unique saved consumers', all(len(fighter.get_components_by_class(cls))==1 for cls in [unreal.CombatPoseConsumer,unreal.CombatAudioConsumer,unreal.CombatRangedVisualConsumer]))
check('A2 controller binds exactly two source dispatchers',pc.get_feedback_binding_count()==2)
for key in ['report','r']:
    if key in globals():
        globals()[key]['A2_binding']='saved production assets; isolated agent setup was not run'
        globals()[key]['A2_fixture']='old positional Rotator corrected; E input/CONTRACT labels retained'
        for stale in ['test_binding','final_A_integration','fixture']:
            if stale in globals()[key]: globals()[key][stale]='A2 production integration replay; see A2_binding'
write()
'''
wrapper.write_text('from pathlib import Path\n__file__='+repr(str(root/src))+'\n'+wrapper_body,encoding='utf-8')
setup_file=archive/'setup.py'
setup_text="exec(compile(Path("+repr(str(root/'Scripts/CombatFeedback/A2_setup.py'))+").read_text(encoding='utf-8'),'A2_setup','exec'))\n"
if case.startswith('e-'):setup_text+="play.set_editor_property('NewWindowWidth',1920)\nplay.set_editor_property('NewWindowHeight',1078)\n"
setup_file.write_text('from pathlib import Path\n'+setup_text,encoding='utf-8')
client=UnrealMCP();editor='EditorToolset.EditorAppToolset';owns=False;setup=False
def running():return json.loads(client.tool(editor,'IsPIERunning')['content'][0]['text'])['returnValue']
def execute(path,name):
    result=run(path);(archive/(name+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert result['success'],result
try:
    assert not running(),'Existing PIE is owned by another run'
    execute(setup_file,'setup');setup=True
    client.tool(editor,'StartPIE',{'options':{'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':1}});owns=True
    execute(wrapper,'launch');deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:report=json.loads(report_path.read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):time.sleep(.4);continue
        if report['status']!='running':break
        time.sleep(.4)
    else:raise TimeoutError(case)
    if report_path!=archive/'report.json':shutil.copy2(report_path,archive/'report.json')
    if case=='d-main' and (root/'Saved/FeedbackD/D-real-combat.wav').exists():shutil.copy2(root/'Saved/FeedbackD/D-real-combat.wav',archive/'recording.wav')
    meta.update(status=report['status'],checks=len(report['checks']),failed=[x for x in report['checks'] if not x['passed']],error=report.get('error'))
    print(json.dumps({'archive':str(archive),**meta},ensure_ascii=False,indent=2))
    assert report['status']=='passed'
except Exception:
    meta['status']='failed';meta['runner_error']=traceback.format_exc()
    if report_path.exists() and report_path!=archive/'report.json':shutil.copy2(report_path,archive/'report.json')
    raise
finally:
    if owns and running():client.tool(editor,'StopPIE')
    if setup:execute(root/'Scripts/CombatFeedback/A2_cleanup.py','cleanup')
    if case.startswith('e-'):
        if old_pointer is not None:e_pointer.write_bytes(old_pointer)
        elif e_pointer.exists():e_pointer.unlink()
    (archive/'meta.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
