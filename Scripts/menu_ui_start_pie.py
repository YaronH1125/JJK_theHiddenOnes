import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
settings = unreal.load_object(None, '/Script/UnrealEd.Default__LevelEditorPlaySettings')
settings.set_editor_property('NewWindowWidth', 1920)
settings.set_editor_property('NewWindowHeight', 1080)
settings.set_editor_property('CenterNewWindow', True)
print('MENU_PIE_WINDOW_CONFIGURED: 1920 x 1080; start PlayMode_InEditorFloating through MCP')
