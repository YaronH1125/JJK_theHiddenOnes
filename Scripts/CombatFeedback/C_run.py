"""Run C's real ranged visual PIE cases, then stop PIE and restore test-only binding.

From project root: python Scripts/CombatFeedback/C_run.py
Requires idle UE Editor on L_DojoArena. Never saves shared assets.
Report: Saved/FeedbackC/pie-report.json.
"""
import json
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ue_mcp import UnrealMCP
from ue_python import run

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/FeedbackC'
case = sys.argv[1] if len(sys.argv) > 1 else 'main'
assert case in ['main', 'extra', 'visual']
script = {'main':'C_pie.py', 'extra':'C_extra_pie.py', 'visual':'C_visual_pie.py'}[case]
report_name = {'main':'pie-report.json', 'extra':'extra-report.json', 'visual':'visual-report.json'}[case]
client = UnrealMCP()
editor = 'EditorToolset.EditorAppToolset'
def running():
    return json.loads(client.tool(editor, 'IsPIERunning')['content'][0]['text'])['returnValue']

owns = False
setup_done = False
try:
    assert not running(), 'Another PIE owns the editor'
    setup = run(root / 'Scripts/CombatFeedback/C_setup.py')
    assert setup['success'], setup
    setup_done = True
    client.tool(editor, 'StartPIE', {'options': {'bSimulate': False, 'playMode': 'PlayMode_InViewPort', 'warmupSeconds': 1}})
    owns = True
    launch = run(root / 'Scripts/CombatFeedback' / script)
    (out / (case + '-launch.json')).write_text(json.dumps(launch, ensure_ascii=False, indent=2), encoding='utf-8')
    assert launch['success'], launch
    deadline = time.monotonic() + 240
    while time.monotonic() < deadline:
        try: report = json.loads((out / report_name).read_text(encoding='utf-8'))
        except (FileNotFoundError, json.JSONDecodeError):
            time.sleep(.3); continue
        if report['status'] != 'running': break
        time.sleep(.4)
    else: raise TimeoutError('C PIE suite exceeded 240 seconds')
    print(json.dumps({'status': report['status'], 'checks': len(report['checks']),
                      'failed': [x for x in report['checks'] if not x['passed']],
                      'error': report.get('error')}, ensure_ascii=False, indent=2))
    assert report['status'] == 'passed'
finally:
    if owns and running(): client.tool(editor, 'StopPIE')
    if setup_done:
        cleanup = run(root / 'Scripts/CombatFeedback/C_cleanup.py')
        (out / (case + '-cleanup.json')).write_text(json.dumps(cleanup, ensure_ascii=False, indent=2), encoding='utf-8')
        assert cleanup['success'], cleanup
