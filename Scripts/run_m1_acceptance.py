"""Run full PIE regression then three fresh-entry checks, leaving PIE stopped."""
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
    mcp.tool(editor, 'StartPIE', {'options': {'bSimulate':False, 'playMode':'PlayMode_InViewPort', 'warmupSeconds':1}})

def execute(name):
    result = run(project / 'Scripts' / name)
    assert result['success'], result

try:
    if running():
        mcp.tool(editor, 'StopPIE')
    start()
    execute('M1_acceptance.py')
    report = project / 'Saved/M1_acceptance.json'
    deadline = time.monotonic() + 160
    while time.monotonic() < deadline:
        state = json.loads(report.read_text(encoding='utf-8'))
        if state['status'] != 'running':
            break
        time.sleep(1)
    assert state['status'] == 'passed', state
    evidence = project / 'Docs/开发过程/验收记录'
    (evidence / 'M1_Acceptance.json').write_text(report.read_text(encoding='utf-8'), encoding='utf-8')
    print('Full acceptance passed:', len(state['checks']), flush=True)
    mcp.tool(editor, 'StopPIE')
    (project / 'Saved/M1_reentry.json').write_text('[]', encoding='utf-8')
    for cycle in range(3):
        start()
        execute('M1_reentry.py')
        print('Fresh entry passed:', cycle + 1, flush=True)
        mcp.tool(editor, 'StopPIE')
    (evidence / 'M1_Reentry.json').write_text((project / 'Saved/M1_reentry.json').read_text(encoding='utf-8'), encoding='utf-8')
finally:
    if running():
        mcp.tool(editor, 'StopPIE')
