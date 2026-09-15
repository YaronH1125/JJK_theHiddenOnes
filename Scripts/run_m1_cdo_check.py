"""Change a CDO, immediately verify PIE, and restore in the same editor session."""
import json
from pathlib import Path
from ue_mcp import UnrealMCP
from ue_python import run
root = Path(__file__).resolve().parents[1]
request = root / 'Saved/M1_cdo_request.json'
def phase(name):
    request.write_text(json.dumps({'phase':name}), encoding='utf-8')
    result = run(root / 'Scripts/M1_cdo_roundtrip.py')
    assert result['success'], result
mcp = UnrealMCP()
editor = 'EditorToolset.EditorAppToolset'
active = False
try:
    phase('modify')
    mcp.tool(editor, 'StartPIE', {'options': {'bSimulate':False,'playMode':'PlayMode_InViewPort','warmupSeconds':1}})
    active = True
    phase('verify')
finally:
    if active:
        mcp.tool(editor, 'StopPIE')
    phase('restore')
result = json.loads((root / 'Saved/M1_cdo_result.json').read_text())
result['restored_margin'] = json.loads((root / 'Saved/M1_cdo_original.json').read_text())['margin']
(root / 'Docs/开发过程/验收记录/M1_CDO_Roundtrip.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(result)
