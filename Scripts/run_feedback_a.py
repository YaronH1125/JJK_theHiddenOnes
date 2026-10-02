"""Run one Feedback A PIE script, stop PIE on every exit; no asset saves.

Usage: python Scripts/run_feedback_a.py baseline_samples
       python Scripts/run_feedback_a.py regression
Requires the project's Editor and local MCP server, with no other editor task running.
Evidence: Saved/FeedbackA/<name>.json. Tests restore temporary settings before exit.
"""
import json
import sys
import time
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run

root = Path(__file__).resolve().parents[1]
name = sys.argv[1] if len(sys.argv) > 1 else 'regression'
client = UnrealMCP()
editor = 'EditorToolset.EditorAppToolset'
def running():
    return json.loads(client.tool(editor, 'IsPIERunning')['content'][0]['text'])['returnValue']
owns_pie = False
try:
    assert not running(), 'Another PIE is active; finish it first'
    client.tool(editor, 'StartPIE', {'options': {'bSimulate': False, 'playMode': 'PlayMode_InViewPort', 'warmupSeconds': 1}})
    owns_pie = True
    result = run(root / f'Scripts/feedback_a_{name}.py')
    assert result['success'], result
    deadline = time.monotonic() + 480
    while time.monotonic() < deadline:
        try:
            report = json.loads((root / f'Saved/FeedbackA/{name}.json').read_text(encoding='utf-8'))
        except (FileNotFoundError, json.JSONDecodeError):
            time.sleep(.5)
            continue
        if report['status'] != 'running':
            break
        time.sleep(.5)
    else:
        raise TimeoutError(f'{name} timeout')
    print(json.dumps(report, indent=2, ensure_ascii=False))
    assert report['status'] == 'passed'
finally:
    if owns_pie and running():
        client.tool(editor, 'StopPIE')
