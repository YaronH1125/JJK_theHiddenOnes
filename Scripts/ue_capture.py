"""Capture the UE viewport through MCP without desktop focus."""
import base64
import json
from pathlib import Path
import sys
from ue_mcp import UnrealMCP


def find_image(value):
    if isinstance(value, str):
        try:
            return find_image(json.loads(value))
        except (ValueError, TypeError):
            return None
    if isinstance(value, dict):
        if value.get('mimeType', '').startswith('image/') and value.get('data'):
            return value['data']
        for child in value.values():
            found = find_image(child)
            if found:
                return found
    if isinstance(value, list):
        for child in value:
            found = find_image(child)
            if found:
                return found
    return None


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    args = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    method = sys.argv[3] if len(sys.argv) > 3 else 'CaptureViewport'
    if method == 'CaptureViewport':
        args.setdefault('captureTransform', None)
        args.setdefault('annotations', None)
        args.setdefault('bShowUI', False)
    result = UnrealMCP().tool('EditorToolset.EditorAppToolset', method, args)
    data = find_image(result)
    assert data, 'MCP returned no image'
    output = Path(sys.argv[1]).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base64.b64decode(data))
    print(output)
