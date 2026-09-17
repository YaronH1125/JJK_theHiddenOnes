
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0)
gm=pc.get_training_game_mode()
p1=gm.get_player_fighter()
mesh=p1.mesh
LINES=[]
LINES.append('MESH=%s'%mesh.get_editor_property('skeletal_mesh_asset'))
LINES.append('ANIMBP=%s'%mesh.get_editor_property('anim_class'))

hud=pc.get_editor_property('combat_hud')
LINES.append('HUD=%s'%hud)
if hud: LINES.append('HUD_CLASS=%s'%hud.get_class())
