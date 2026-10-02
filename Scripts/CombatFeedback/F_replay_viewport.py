"""F-only fixture correction for fresh E replays; do not change E assertions/code.

Usage: python Scripts/CombatFeedback/F_replay_viewport.py e-main (or e-edges/e-reentry).
Current desktop measured 1920x1081 with A's 1078 window height. Use 1077 for
exact 1080 viewport, keep failed run and repeat. Production source stays byte
identical; original A runner is executed in memory with only this window setup
correction. Evidence gets a separate F index and an explicit fixture label.
"""
import datetime
import json
from pathlib import Path
import shutil
import sys
ROOT=Path(__file__).resolve().parents[2]
case=sys.argv[1];assert case in ['e-main','e-edges','e-reentry']
parent=ROOT/'Saved/FeedbackA2/replays';before=set(parent.iterdir())
source=ROOT/'Scripts/CombatFeedback/A2_replay.py'
text=source.read_text(encoding='utf-8')
assert "('NewWindowHeight',1078)" in text
text=text.replace("('NewWindowHeight',1078)","('NewWindowHeight',1077)")
try:
    exec(compile(text,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
finally:
    fresh=sorted(set(parent.iterdir())-before)
    if len(fresh)==1:
        dest=ROOT/'Saved/CombatFeedback/F/viewport-retest'/fresh[0].name
        dest.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(fresh[0],dest)
        (dest/'F-fixture.json').write_text(json.dumps({'candidate':'FA2-20260930-v2','case':case,
            'change':'F fixture window height 1078 -> 1077; E gameplay/assertions unchanged',
            'previous_failure':'10-e-main actual viewport 1920x1081','human_input':False},indent=2),encoding='utf-8')
        print('F corrected-viewport evidence: '+str(dest))
