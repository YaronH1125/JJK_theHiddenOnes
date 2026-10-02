"""Read-only R1 saved dependency audit; preserves the historical A2 audit."""
from pathlib import Path
import unreal
source = Path(unreal.Paths.project_dir())/'Scripts/CombatFeedback/A2_audit.py'
text = source.read_text(encoding='utf-8').replace("/'FeedbackA2/dependency-audit.json'", "/'FeedbackRevisionR1/dependency-audit.json'")
exec(compile(text,str(source),'exec'))
assert '/Game/CombatFeedback/VFX/PS_R1_Beam_Frost' in graph
assert fd.mobile_blast.beam_effect == fd.super_blast.beam_effect
assert fd.mobile_blast.beam_effect.get_path_name().startswith('/Game/CombatFeedback/VFX/PS_R1_Beam_Frost.')
