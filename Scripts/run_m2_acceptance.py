"""Run M2 PIE acceptance (T01-T12), leaving PIE stopped."""
import json
from pathlib import Path
import time
import hashlib
import subprocess
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
    # 内存中可加载不等于资产已交付；运行前验证正式文件及 Git ignore 规则。
    assets = {}
    for name in ('IA_Attack', 'IMC_Training', 'BP_ArenaPlayerController'):
        path = project / f'Content/Training/{name}.uasset'
        assert path.is_file() and path.stat().st_size > 0, f'Missing asset on disk: {path}'
        ignored = subprocess.run(['git', 'check-ignore', '--no-index', str(path)], cwd=project,
                                 capture_output=True, text=True)
        assert ignored.returncode == 1, f'Asset ignored or git check failed: {ignored.stdout}{ignored.stderr}'
        assets[name] = {'path': path.relative_to(project).as_posix(), 'bytes': path.stat().st_size,
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
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
    evidence = project / 'Docs/开发过程/验收记录'
    assert state is not None, 'No M2 report'
    state['disk_assets'] = assets
    assert state['status'] == 'passed' and len(state['checks']) == 57 and all(c['passed'] for c in state['checks']), state
    (evidence / 'M2_Acceptance.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
    print('M2 acceptance passed:', len(state['checks']), flush=True)
finally:
    if running():
        mcp.tool(editor, 'StopPIE')
