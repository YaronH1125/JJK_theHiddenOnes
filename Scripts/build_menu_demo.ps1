param([switch]$ReuseCook)
$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
$menuArchive = Join-Path $menuRoot ('Saved/Packages/MenuUI_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$menuLog = Join-Path $menuRoot 'Saved/MenuDemoBuildCookRun.log'
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close the Editor before building the menu demo.' }
& python -X utf8 (Join-Path $PSScriptRoot 'build_menu_ui.py')
if ($LASTEXITCODE -ne 0) { throw 'Menu export failed.' }
$menuCook = if ($ReuseCook) { '-skipcook' } else { '-cook' }
& 'F:/GameStudy/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$menuRoot/JJK_theHiddenOnes.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor $menuCook '-map=/Game/Maps/L_DojoArena+/Game/Maps/L_TrainingArena' -stage -pak -archive "-archivedirectory=$menuArchive" -unattended -utf8output *> $menuLog
if ($LASTEXITCODE -ne 0) { throw "Menu demo build failed ($LASTEXITCODE); see $menuLog" }
Write-Output "MENU_DEMO_PACKAGE=$menuArchive/Windows/JJK_theHiddenOnes.exe"
