"""Validate the staged local menu without an editor or design server."""
import argparse
import json
import subprocess
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('exe', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / 'Saved/MenuPackageSmoke' / time.strftime('%Y%m%d_%H%M%S')
out.mkdir(parents=True)
exe = args.exe.resolve()
assert exe.is_file(), exe
command = [str(exe), '-unattended', '-windowed', '-ResX=1920', '-ResY=1080',
           '-UserDir=' + (out / 'User').as_posix(), '-abslog=' + (out / 'runtime.log').as_posix(),
           '-JJKMenuSmokeOut=' + out.as_posix()]
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
result = subprocess.run(command, cwd=root, startupinfo=startup, timeout=180)
report = json.loads((out / 'report.json').read_text(encoding='utf-8'))
report.update(exit_code=result.returncode, command=command, output=str(out))
report['passed'] = result.returncode == 0 and report['ready'] and report['paused'] and report['page'] == 'main' and report['screenshot']
(out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
assert report['passed'], report
