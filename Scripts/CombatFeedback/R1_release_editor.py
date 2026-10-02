"""Release clean R1 editor; leaves historical A2/F handoff records intact."""
from pathlib import Path
import unreal
source = Path(unreal.Paths.project_dir())/'Scripts/CombatFeedback/F_release_editor.py'
text = source.read_text(encoding='utf-8').replace("/'CombatFeedback/F/editor-handoff.json'", "/'FeedbackRevisionR1/editor-handoff.json'")
exec(compile(text,str(source),'exec'))
