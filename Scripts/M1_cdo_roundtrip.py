"""Editor-side CDO regression phase, selected by Saved/M1_cdo_request.json."""
import json
from pathlib import Path
import unreal
saved = Path(unreal.Paths.project_saved_dir())
phase = json.loads((saved / 'M1_cdo_request.json').read_text())['phase']
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if phase == 'verify':
    assert world
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    gm = pc.get_training_game_mode()
    value = gm.get_editor_property('spawn_safety_margin')
    initial = gm.get_player_fighter().get_initial_transform().translation.z
    # Character initialization may settle onto the floor before its transform is recorded.
    assert value == 7 and initial >= 96, (value, initial)
    (saved / 'M1_cdo_result.json').write_text(json.dumps({'same_session_pie':True,
        'modified_margin':value, 'spawn_initial_z':initial}), encoding='utf-8')
else:
    assert world is None
    bp = unreal.load_asset('/Game/Training/BP_TrainingGameMode')
    cdo = unreal.get_default_object(bp.generated_class())
    if phase == 'modify':
        (saved / 'M1_cdo_original.json').write_text(json.dumps({'margin':cdo.get_editor_property('spawn_safety_margin')}))
        value = 7
    else:
        value = json.loads((saved / 'M1_cdo_original.json').read_text())['margin']
    cdo.set_editor_property('spawn_safety_margin', value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    assert unreal.get_default_object(bp.generated_class()).get_editor_property('spawn_safety_margin') == value
    print('CDO', phase, value)
