"""Archive an existing production-binding replay as R1 evidence.

Usage: python Scripts/CombatFeedback/R1_replay.py a-edge|c-main|e-main|e-edges|run-shift|run-integrated
Only adapts E's window fixture to an actual 1920x1080 viewport. Assertions stay
unchanged. Historical A2 evidence remains intact; each execution has new paths.
"""
import datetime
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
case = sys.argv[1]
is_run = case.startswith('run-')
parent = root/'Saved/FeedbackA2'/('runs' if is_run else 'replays')
before = set(parent.iterdir())
source = root/'Scripts/CombatFeedback'/('A2_run.py' if is_run else 'A2_replay.py')
if is_run:
    sys.argv[1] = case.removeprefix('run-')
body = source.read_text(encoding='utf-8').replace("('NewWindowHeight',1078)", "('NewWindowHeight',1077)")
if case == 'run-integrated':
    body = body.replace("'integrated':'A2_pie.py'", "'integrated':'R1_integrated.py'")
# Window borders/work-area clipping differ from the previous F desktop. Switch
# E's test-only game viewport to borderless 1080p before its unchanged size gate.
if case.startswith('e-'):
    body = body.replace("exec(compile(text,__file__,'exec'))", "R1_EXACT_VIEWPORT=True\nexec(compile(text,__file__,'exec'))")
setup_path = root/'Saved/FeedbackA2/setup.json'
setup_bytes = setup_path.read_bytes() if setup_path.exists() else None
pointer = root/'Saved/FeedbackA2/latest-run.json'
pointer_bytes = pointer.read_bytes() if pointer.exists() else None
try:
    exec(compile(body, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
finally:
    if setup_bytes is not None:
        setup_path.write_bytes(setup_bytes)
    if pointer_bytes is not None:
        pointer.write_bytes(pointer_bytes)
    fresh = sorted(set(parent.iterdir()) - before)
    if len(fresh) == 1:
        target = root/'Saved/FeedbackRevisionR1/regressions'/(fresh[0].name+'-'+case if is_run else fresh[0].name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(fresh[0], target)
        (target/'R1-fixture.json').write_text(json.dumps({
            'candidate': 'FR1-20261001', 'case': case,
            'time': datetime.datetime.now().astimezone().isoformat(),
            'window_fixture': 'E requests 1920x1080; actual viewport gate stays authoritative, including border/work-area failures',
            'human_input': False,
        }, indent=2), encoding='utf-8')
        print('R1 archived replay: '+str(target))
