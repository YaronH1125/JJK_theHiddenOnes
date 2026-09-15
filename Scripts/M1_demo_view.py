"""Frame both fighters in a fresh PIE and capture its actual game viewport."""
from pathlib import Path
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pc.set_control_rotation(unreal.Rotator(pitch=-15, yaw=15, roll=0))
print('DEMO', pc.get_player_fighter().get_actor_location(), pc.get_training_game_mode().get_opponent_fighter().get_actor_location())

counter = {'frames':0}
def capture(_dt):
    counter['frames'] += 1
    if counter['frames'] < 3:
        return
    unreal.unregister_slate_post_tick_callback(counter['handle'])
    path = (Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / 'Docs/开发过程/验收记录/M1_Codex_PIE.png').as_posix()
    unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x720 filename="{path}"')
counter['handle'] = unreal.register_slate_post_tick_callback(capture)
