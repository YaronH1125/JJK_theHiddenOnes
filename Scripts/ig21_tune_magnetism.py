"""Match the Ishigori approach destination to measured KB contact reach."""
import json, shutil
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
root=Path(unreal.Paths.project_dir())
out=root/'Saved/IG21';out.mkdir(exist_ok=True)
backup=out/'DA_Fighter_Ishigori.before.uasset'
if not backup.exists():shutil.copy2(root/'Content/Training/DA_Fighter_Ishigori.uasset',backup)
fd=unreal.load_asset('/Game/Training/DA_Fighter_Ishigori')
settings={'attack_reach':120.0,'magnetism_lunge':250.0,'magnetism_speed':2500.0}
for key,val in settings.items():fd.set_editor_property(key,val)
assert unreal.EditorAssetLibrary.save_loaded_asset(fd,only_if_is_dirty=False)
(out/'settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
print(settings)
