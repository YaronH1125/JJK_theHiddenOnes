"""Snapshot the dirty workspace, not just HEAD. Run outside Unreal."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
kind = sys.argv[1] if len(sys.argv) > 1 else 'baseline'
assert kind in ('baseline', 'candidate')
out = root / 'Saved/FeedbackA2' / kind
assert not (out / 'manifest.json').exists(), 'Do not overwrite a frozen snapshot'
out.mkdir(parents=True, exist_ok=True)
status = subprocess.check_output(['git', 'status', '--short'], cwd=root)
(out / 'git-status.txt').write_bytes(status)
manifest = {'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root).decode().strip(), 'files': {}}
for folder in ('Source', 'Config', 'Content', 'Docs', 'Scripts'):
    for p in sorted((root / folder).rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts:
            continue
        rel = p.relative_to(root).as_posix()
        manifest['files'][rel] = hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
        # Original text and all integration-owned assets are recoverable locally.
        if folder != 'Content' or rel.startswith(('Content/CombatFeedback/', 'Content/Characters/Ishigori/Repaired/Feedback/')) or rel == 'Content/Training/DA_Fighter_Ishigori.uasset':
            dest = out / 'files' / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dest)
dll = root / 'Binaries/Win64/UnrealEditor-JJK_theHiddenOnes.dll'
manifest['editor_dll_sha256'] = hashlib.file_digest(dll.open('rb'), 'sha256').hexdigest() if dll.exists() else None
(out / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'snapshot': str(out), 'files': len(manifest['files']), 'head': manifest['head']}))
