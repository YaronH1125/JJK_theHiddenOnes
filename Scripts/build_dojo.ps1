param([switch]$ReuseCook)
$ErrorActionPreference = 'Stop'
$dojoRoot = Split-Path -Parent $PSScriptRoot
$dojoProject = Join-Path $dojoRoot 'JJK_theHiddenOnes.uproject'
$dojoArchive = Join-Path $dojoRoot 'Saved/Packages/Dojo'
$dojoLog = Join-Path $dojoRoot 'Saved/Dojo_BuildCookRun.log'
New-Item -ItemType Directory -Path (Join-Path $dojoRoot 'Saved') -Force | Out-Null
# Imported packs are deliberately outside Git. Fail early on an incomplete checkout.
$dojoManifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Baselines/external_assets.json') -Raw | ConvertFrom-Json
foreach ($dojoPackage in $dojoManifest.packages.PSObject.Properties) {
    foreach ($dojoAsset in $dojoPackage.Value.files) {
        $dojoAssetPath = Join-Path $dojoRoot $dojoAsset.path
        if (!(Test-Path -LiteralPath $dojoAssetPath -PathType Leaf) -or (Get-Item -LiteralPath $dojoAssetPath).Length -ne $dojoAsset.bytes) {
            throw "Missing or unexpected external asset: $($dojoAsset.path). Restore the asset archive; see Docs/12_版本控制与素材依赖.md."
        }
    }
}
# The whitebox is included deliberately in this validation candidate for same-binary regression.
$dojoCookFlag = if ($ReuseCook) { '-skipcook' } else { '-cook' }
& 'F:/GameStudy/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$dojoProject" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor $dojoCookFlag '-map=/Game/Maps/L_DojoArena+/Game/Maps/L_TrainingArena' -stage -pak -archive "-archivedirectory=$dojoArchive" -unattended -utf8output *> $dojoLog
if ($LASTEXITCODE -ne 0) { throw "Dojo build failed ($LASTEXITCODE). See $dojoLog" }
Write-Output "Dojo candidate: $dojoArchive/Windows"
