# -*- coding: utf-8 -*-
"""幂等修复攻击输入：保存现有内存资产，缺失时用专用工厂创建；失败立即终止。"""
from pathlib import Path
import subprocess
import unreal

assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, 'Stop PIE first'
EAL = unreal.EditorAssetLibrary
ia_path = '/Game/Training/IA_Attack'
# 不删除已有对象：删除再重建会使 IMC/CDO 的引用失效。
ia_attack = unreal.load_object(None, ia_path + '.IA_Attack')
if ia_attack is None:
    ia_attack = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'IA_Attack', '/Game/Training', unreal.InputAction, unreal.InputAction_Factory())
assert ia_attack is not None, 'IA_Attack creation failed'
ia_attack.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
ia_attack.set_editor_property('triggers', [])
ia_attack.set_editor_property('modifiers', [])
assert EAL.save_loaded_asset(ia_attack, only_if_is_dirty=False), 'IA_Attack save failed'
disk = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())) / 'Training/IA_Attack.uasset'
assert disk.is_file() and disk.stat().st_size > 0, f'Asset did not reach Content: {disk}'
git_check = subprocess.run(['git', 'check-ignore', '--no-index', str(disk)], cwd=disk.parents[2],
                           capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
assert git_check.returncode == 1, f'Asset is ignored or Git check failed: {git_check.stdout}{git_check.stderr}'

imc = EAL.load_asset('/Game/Training/IMC_Training')
data = imc.get_editor_property('default_key_mappings')
mappings = [m for m in data.get_editor_property('mappings')
            if str(m.key.get_editor_property('key_name')) != 'LeftMouseButton'
            and m.get_editor_property('action') != ia_attack]
key = unreal.Key()
key.set_editor_property('key_name', 'LeftMouseButton')
mappings.append(unreal.EnhancedActionKeyMapping(action=ia_attack, key=key))
data.set_editor_property('mappings', mappings)
imc.set_editor_property('default_key_mappings', data)
assert EAL.save_loaded_asset(imc, only_if_is_dirty=False), 'IMC save failed'

bp = EAL.load_asset('/Game/Training/BP_ArenaPlayerController')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.get_default_object(bp.generated_class()).set_editor_property('attack_action', ia_attack)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert EAL.save_loaded_asset(bp, only_if_is_dirty=False), 'Controller save failed'
assert unreal.get_default_object(bp.generated_class()).get_editor_property('attack_action') == ia_attack
print('[M2FixInput] Saved input action, mapping and compiled Controller:', str(disk))
