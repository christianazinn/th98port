[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = "0.1.0",
    [string]$OutputDirectory,
    [string]$StandaloneDirectory,
    [string]$RendererPath,
    [string]$FfmpegPath,
    [string]$FfmpegLicensePath,
    [switch]$SkipDependencyInstall
)

$ErrorActionPreference = "Stop"
$toolRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$repoRoot = [IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $toolRoot)))
$sourceRoot = Join-Path $toolRoot "src"
$testRoot = Join-Path $toolRoot "tests"
$scratchRoot = Join-Path $repoRoot "out\tool-build\th03-replay-video-gui"
$venvRoot = Join-Path $scratchRoot "venv"
$python = Join-Path $venvRoot "Scripts\python.exe"

function Assert-Descendant([string]$Path, [string]$Parent, [string]$Label) {
    $resolved = [IO.Path]::GetFullPath($Path)
    $resolvedParent = [IO.Path]::GetFullPath($Parent).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($resolvedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain below $resolvedParent"
    }
    return $resolved
}

if (-not $RendererPath) {
    $RendererPath = Join-Path $repoRoot `
        "out\build\msvc-x64-sdl\Release\th03-replay-video.exe"
    if (-not [IO.File]::Exists($RendererPath)) {
        & cmake --build (Join-Path $repoRoot "out\build\msvc-x64-sdl") `
            --config Release --target th03-replay-video --parallel
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to build th03-replay-video."
        }
    }
}
$rendererFull = [IO.Path]::GetFullPath($RendererPath)
if (-not [IO.File]::Exists($rendererFull)) {
    throw "Replay renderer does not exist: $rendererFull"
}
if ([bool]$FfmpegPath -ne [bool]$FfmpegLicensePath) {
    throw "Bundling FFmpeg requires both -FfmpegPath and -FfmpegLicensePath."
}
if ($FfmpegPath) {
    $ffmpegFull = [IO.Path]::GetFullPath($FfmpegPath)
    $ffmpegLicenseFull = [IO.Path]::GetFullPath($FfmpegLicensePath)
    if (-not [IO.File]::Exists($ffmpegFull) -or
        -not [IO.File]::Exists($ffmpegLicenseFull)) {
        throw "The FFmpeg binary or matching license does not exist."
    }
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot `
        "out\packages\th03-replay-renderer-v$Version"
}
$outputFull = Assert-Descendant $OutputDirectory $repoRoot "OutputDirectory"
if (-not $StandaloneDirectory) {
    $StandaloneDirectory = Join-Path $repoRoot "out\standalone"
}
$standaloneFull = Assert-Descendant `
    $StandaloneDirectory $repoRoot "StandaloneDirectory"
if ([IO.Directory]::Exists($outputFull)) {
    Remove-Item -LiteralPath $outputFull -Recurse -Force
}
[IO.Directory]::CreateDirectory($outputFull) | Out-Null

if (-not [IO.File]::Exists($python)) {
    [IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
    & python -m venv $venvRoot
    if ($LASTEXITCODE -ne 0) { throw "Failed to create Python environment." }
}
if (-not $SkipDependencyInstall) {
    & $python -m pip install --disable-pip-version-check `
        -r (Join-Path $toolRoot "requirements-build.txt")
    if ($LASTEXITCODE -ne 0) { throw "Failed to install GUI build dependencies." }
}

$oldPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $sourceRoot
    & $python -m unittest discover -s $testRoot -v
    if ($LASTEXITCODE -ne 0) { throw "Replay Renderer GUI tests failed." }
} finally {
    $env:PYTHONPATH = $oldPythonPath
}

$workPath = Assert-Descendant (Join-Path $scratchRoot "pyinstaller-work") $repoRoot "work path"
$specPath = Assert-Descendant (Join-Path $scratchRoot "pyinstaller-spec") $repoRoot "spec path"
foreach ($path in ($workPath, $specPath)) {
    if ([IO.Directory]::Exists($path)) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($path) | Out-Null
}
$embeddedNativeRoot = Assert-Descendant `
    (Join-Path $scratchRoot "embedded-native") $repoRoot "embedded native path"
if ([IO.Directory]::Exists($embeddedNativeRoot)) {
    Remove-Item -LiteralPath $embeddedNativeRoot -Recurse -Force
}
[IO.Directory]::CreateDirectory($embeddedNativeRoot) | Out-Null
$embeddedRenderer = Join-Path $embeddedNativeRoot "th03-replay-video.exe"
Copy-Item -LiteralPath $rendererFull -Destination $embeddedRenderer
$binaryArguments = @("--add-binary", "$embeddedRenderer;th03_native")
if ($FfmpegPath) {
    $binaryArguments += @("--add-binary", "$ffmpegFull;th03_ffmpeg")
}

& $python -m PyInstaller `
    --noconfirm --clean --onefile --windowed `
    --name TH03ReplayRenderer `
    --version-file (Join-Path $toolRoot "version_info.txt") `
    --paths $sourceRoot `
    --collect-all tkinterdnd2 `
    @binaryArguments `
    --distpath $outputFull `
    --workpath $workPath `
    --specpath $specPath `
    (Join-Path $toolRoot "launcher.py")
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed." }

Copy-Item -LiteralPath (Join-Path $toolRoot "README.md") `
    -Destination (Join-Path $outputFull "README.md")
Copy-Item -LiteralPath (Join-Path $toolRoot "THIRD_PARTY_NOTICES.txt") `
    -Destination (Join-Path $outputFull "THIRD_PARTY_NOTICES.txt")
$licenseRoot = Join-Path $outputFull "licenses"
[IO.Directory]::CreateDirectory($licenseRoot) | Out-Null
foreach ($dependency in ("98fmplayer", "ymfm", "TinySoundFont")) {
    $destination = Join-Path $licenseRoot $dependency
    [IO.Directory]::CreateDirectory($destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\$dependency\LICENSE") `
        -Destination (Join-Path $destination "LICENSE.txt")
}
$basePrefix = (& $python -c "import sys; print(sys.base_prefix)").Trim()
$sitePackages = Join-Path $venvRoot "Lib\site-packages"
$runtimeLicenses = @(
    [PSCustomObject]@{
        Name = "Python"; Source = Join-Path $basePrefix "LICENSE.txt"
    },
    [PSCustomObject]@{
        Name = "Tk"; Source = Join-Path $basePrefix "tcl\tk8.6\license.terms"
    },
    [PSCustomObject]@{
        Name = "PyInstaller"; Source = Join-Path $sitePackages `
            "pyinstaller-6.16.0.dist-info\licenses\COPYING.txt"
    },
    [PSCustomObject]@{
        Name = "tkinterdnd2"; Source = Join-Path $sitePackages `
            "tkinterdnd2-0.6.2.dist-info\licenses\LICENSE"
    },
    [PSCustomObject]@{
        Name = "TkDND"; Source = Join-Path $toolRoot "licenses\TkDND\LICENSE.txt"
    }
)
foreach ($license in $runtimeLicenses) {
    $name = $license.Name
    $source = $license.Source
    if (-not [IO.File]::Exists($source)) {
        throw "Required $name license is missing: $source"
    }
    $destination = Join-Path $licenseRoot $name
    [IO.Directory]::CreateDirectory($destination) | Out-Null
    Copy-Item -LiteralPath $source `
        -Destination (Join-Path $destination "LICENSE.txt")
}
if ($FfmpegPath) {
    $destination = Join-Path $licenseRoot "FFmpeg"
    [IO.Directory]::CreateDirectory($destination) | Out-Null
    Copy-Item -LiteralPath $ffmpegLicenseFull `
        -Destination (Join-Path $destination "LICENSE.txt")
}

$exe = Join-Path $outputFull "TH03ReplayRenderer.exe"
$smoke = Start-Process -FilePath $exe -ArgumentList "--gui-smoke" `
    -WindowStyle Hidden -Wait -PassThru
if ($smoke.ExitCode -ne 0) {
    throw "Packaged GUI smoke failed with exit code $($smoke.ExitCode)."
}
[IO.Directory]::CreateDirectory($standaloneFull) | Out-Null
Copy-Item -LiteralPath $exe `
    -Destination (Join-Path $standaloneFull "TH03ReplayRenderer.exe") -Force

$manifest = @(
    Get-ChildItem -LiteralPath $outputFull -File -Recurse | Sort-Object FullName |
        ForEach-Object {
            $relative = [IO.Path]::GetRelativePath($outputFull, $_.FullName).Replace('\', '/')
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$relative`t$($_.Length)`t$hash"
        }
)
[IO.File]::WriteAllLines((Join-Path $outputFull "MANIFEST.tsv"), $manifest)

$zip = "$outputFull.zip"
if ([IO.File]::Exists($zip)) { Remove-Item -LiteralPath $zip -Force }
Push-Location $outputFull
try {
    & cmake -E tar cf $zip --format=zip .
    if ($LASTEXITCODE -ne 0) { throw "Failed to create package ZIP." }
} finally {
    Pop-Location
}
Write-Output "package=$outputFull"
Write-Output "zip=$zip"
Write-Output "standalone=$(Join-Path $standaloneFull 'TH03ReplayRenderer.exe')"
