# Build the frozen R1 into a distinct archive. Preserve all old playable packages.
$ErrorActionPreference = 'Stop'
$r1Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$r1Out = Join-Path $r1Root 'Saved/FeedbackRevisionR1'
$r1Archive = Join-Path $r1Root 'Saved/Packages/FeedbackR1_20261002'
$r1Log = Join-Path $r1Out 'BuildCookRun.log'
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Editor before building' }
& python -X utf8 (Join-Path $PSScriptRoot 'R1_snapshot.py') --verify
if ($LASTEXITCODE -ne 0) { throw 'Frozen R1 input mismatch' }
if (Test-Path -LiteralPath (Join-Path $r1Archive 'Windows/JJK_theHiddenOnes.exe')) { throw 'Preserve existing package; use a distinct archive' }
& 'F:/GameStudy/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$r1Root/JJK_theHiddenOnes.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor -cook '-map=/Game/Maps/L_DojoArena+/Game/Maps/L_TrainingArena' -stage -pak -archive "-archivedirectory=$r1Archive" -unattended -utf8output *> $r1Log
if ($LASTEXITCODE -ne 0) { throw "R1 BuildCookRun failed ($LASTEXITCODE); see $r1Log" }
Write-Output "New R1 package: $r1Archive/Windows"
