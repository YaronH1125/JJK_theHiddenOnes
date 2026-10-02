"""R1 container / hard-reference audit; retains historical F evidence."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
source = root/'Scripts/CombatFeedback/F_cook_audit.py'
text = source.read_text(encoding='utf-8')
text = text.replace('Saved/Packages/FeedbackF_FA2_v2', 'Saved/Packages/FeedbackR1_20261002')
text = text.replace('Saved/CombatFeedback/F', 'Saved/FeedbackRevisionR1')
text = text.replace('Saved/FeedbackA2/dependency-audit.json', 'Saved/FeedbackRevisionR1/dependency-audit.json')
text = text.replace('FA2-20260930-v2', 'FR1-20261001')
scope = {'__file__':str(source),'__name__':'__main__'}
exec(compile(text,str(source),'exec'), scope)
assert '/Game/CombatFeedback/VFX/PS_R1_Beam_Frost' in scope['present']
assert len(scope['report']['maps']) == 2
