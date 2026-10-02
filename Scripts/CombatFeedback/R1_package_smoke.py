"""Fresh graphical + audio package smoke with HitStop enabled for R1.

Preserves old F reports/packages. Uses the existing explicitly labelled debug
fixture, not human play or normal-resource three AI matches.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
source = root/'Scripts/CombatFeedback/F_package_smoke.py'
text = source.read_text(encoding='utf-8')
text = text.replace('Saved/Packages/FeedbackF_FA2_v2', 'Saved/Packages/FeedbackR1_20261002')
text = text.replace('Saved/CombatFeedback/F/package-smoke', 'Saved/FeedbackRevisionR1/package-smoke')
text = text.replace('FA2-20260930-v2', 'FR1-20261001').replace('JJK.Feedback.HitStop 0', 'JJK.Feedback.HitStop 1')
text = text.replace("    print(json.dumps(", "    assert state['status']=='passed' and not state['errors'], state\n    print(json.dumps(")
exec(compile(text,str(source),'exec'), {'__file__':str(source),'__name__':'__main__'})
