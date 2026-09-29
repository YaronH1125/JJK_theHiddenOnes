"""Run the melee regression in a fresh PIE, without saving temporary test state."""
import json
import time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run

root = Path(__file__).resolve().parents[1]
client = UnrealMCP()
editor = 'EditorToolset.EditorAppToolset'
def running():
    return json.loads(client.tool(editor, 'IsPIERunning')['content'][0]['text'])['returnValue']
try:
    if running():
        client.tool(editor, 'StopPIE')
    client.tool(editor, 'StartPIE', {'options': {'bSimulate': False, 'playMode': 'PlayMode_InViewPort', 'warmupSeconds': 1}})
    result = run(root / 'Scripts/ig20_acceptance.py')
    assert result['success'], result
    deadline = time.monotonic() + 180
    while time.monotonic() < deadline:
        try:
            report = json.loads((root / 'Saved/IG20/acceptance.json').read_text(encoding='utf-8'))
        except json.JSONDecodeError:
            time.sleep(.5)
            continue
        if report['status'] != 'running':
            break
        time.sleep(.5)
    else:
        raise TimeoutError('IG20 runner timeout')
    print(json.dumps({'status': report['status'], 'checks': len(report['checks']),
                      'failed': [c for c in report['checks'] if not c['passed']],
                      'error': report.get('error'), 'seconds': report.get('duration_seconds')}, indent=2))
    assert report['status'] == 'passed'
finally:
    if running():
        client.tool(editor, 'StopPIE')
