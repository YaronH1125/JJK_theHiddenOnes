"""Import only the v9 HUD textures; safe to rerun with PIE stopped."""
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before importing HUD textures'
root = Path(unreal.Paths.project_dir()) / 'Docs/assets/hud-v9'
names = ['scroll', 'portrait', 'punch', 'heavy', 'kick', 'hkick', 'swap', 'blast', 'sblast', 'aim', 'domain', 'mouse']
tasks = []
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(root / (name + '.png'))
    task.destination_path = '/Game/UI/HUD/V9'
    task.destination_name = 'T_' + name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in names:
    asset = unreal.load_asset('/Game/UI/HUD/V9/T_' + name)
    assert isinstance(asset, unreal.Texture2D), name
    asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    asset.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    asset.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    asset.set_editor_property('srgb', True)
    asset.set_editor_property('never_stream', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), name
print('HUD_V9_IMPORTED: 12 textures')
