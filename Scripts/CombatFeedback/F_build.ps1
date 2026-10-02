# Build A's frozen local candidate into a distinct F archive. No ReuseCook.
# Usage from project root: powershell -File Scripts/CombatFeedback/F_build.ps1
# The legacy external-asset manifest disagrees with the already frozen Frost
# beam. Preserve/report that mismatch; validate the exact A candidate instead.
$ErrorActionPreference = 'Stop'
$fRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fOut = Join-Path $fRoot 'Saved/CombatFeedback/F'
$fArchive = Join-Path $fRoot 'Saved/Packages/FeedbackF_FA2_v2'
$fLog = Join-Path $fOut 'BuildCookRun.log'
New-Item -ItemType Directory -Path $fOut -Force | Out-Null
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Editor before building' }
& python (Join-Path $PSScriptRoot 'F_snapshot.py') --verify
if ($LASTEXITCODE -ne 0) { throw 'Frozen candidate mismatch; do not build a different version' }
if (Test-Path -LiteralPath (Join-Path $fArchive 'Windows/JJK_theHiddenOnes.exe')) { throw 'Preserve existing F package; select a new archive for a new build' }
& 'F:/GameStudy/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$fRoot/JJK_theHiddenOnes.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor -cook '-map=/Game/Maps/L_DojoArena+/Game/Maps/L_TrainingArena' -stage -pak -archive "-archivedirectory=$fArchive" -unattended -utf8output *> $fLog
if ($LASTEXITCODE -ne 0) { throw "F BuildCookRun failed ($LASTEXITCODE); see $fLog" }
Write-Output "F same-candidate package: $fArchive/Windows"
