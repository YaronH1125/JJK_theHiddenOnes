"""Run M2 PIE acceptance (T01-T12), leaving PIE stopped."""
import json
from pathlib import Path
import time
from ue_mcp import UnrealMCP
from ue_python import run

project = Path(__file__).resolve().parents[1]
mcp = UnrealMCP()
editor = 'EditorToolset.EditorAppToolset'

def running():
    return json.loads(mcp.tool(editor, 'IsPIERunning')['content'][0]['text'])['returnValue']

def start():
    mcp.tool(editor, 'StartPIE', {'options': {'bSimulate': False, 'playMode': 'PlayMode_InViewPort', 'warmupSeconds': 1}})

def execute(name):
    result = run(project / 'Scripts' / name)
    assert result['success'], result

try:
    if running():
        mcp.tool(editor, 'StopPIE')
    start()
    execute('M2_acceptance.py')
    report = project / 'Saved/M2_acceptance.json'
    state = None
    deadline = time.monotonic() + 240
    while time.monotonic() < deadline:
        state = json.loads(report.read_text(encoding='utf-8'))
        if state['status'] != 'running':
            break
        time.sleep(1)
    assert state and state['status'] == 'passed', state
    evidence = project / 'Docs/开发过程/验收记录'
    (evidence / 'M2_Acceptance.json').write_text(report.read_text(encoding='utf-8'), encoding='utf-8')
    print('M2 acceptance passed:', len(state['checks']), flush=True)
finally:
    if running():
        mcp.tool(editor, 'StopPIE')
