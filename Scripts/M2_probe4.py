# -*- coding: utf-8 -*-
import unreal
TAG = "[M2Probe4]"
def log(m): unreal.log(f"{TAG} {m}")
sub = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
log("sub methods: " + str([x for x in dir(sub) if not x.startswith('_')]))
at = unreal.AssetToolsHelpers.get_asset_tools()
log("at methods: " + str([x for x in dir(at) if not x.startswith('_')]))
