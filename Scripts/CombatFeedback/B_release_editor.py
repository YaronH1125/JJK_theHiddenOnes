"""Release the B-owned editor after cleanup; never save content or maps.

python Scripts/ue_python.py Scripts/CombatFeedback/B_release_editor.py
Only use after B owns and has finished the Editor. Refuses dirty maps.
"""
from pathlib import Path
import unreal
cleanup=Path(__file__).with_name('B_cleanup.py')
exec(compile(cleanup.read_text(encoding='utf-8'),str(cleanup),'exec'))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Unrelated map edits need their owner'
unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
