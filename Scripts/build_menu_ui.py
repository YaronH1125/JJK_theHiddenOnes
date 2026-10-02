"""Export the approved, self-contained design with the local Unreal bridge."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Docs/assets/menu-ui-v1.0.html'
DEST = ROOT / 'Content/UI/Menu/menu-ui.html'
BRIDGE = ROOT / 'Content/UI/Menu/menu-ue.js'
html = SOURCE.read_text(encoding='utf-8-sig')
assert html.count('</body>') == 1
assert 'window.menuUE=' not in html
html = html.replace('</body>', '<script>\n' + BRIDGE.read_text(encoding='utf-8-sig') + '\n</script>\n</body>')
DEST.write_text(html, encoding='utf-8', newline='\n')
print(f'Exported approved menu and UE bridge: {DEST} ({DEST.stat().st_size} bytes)')
