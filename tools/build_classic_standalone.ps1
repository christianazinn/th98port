#Requires -Version 7.0
[CmdletBinding()]
param(
	[string]$BuildPreset = "msvc-x64-sdl",
	[string]$Configuration = "Release",
	[string]$DataRoot,
	[string]$MainExecutable,
	[string]$MainlExecutable,
	[string]$OpExecutable,
	[string]$FontRom,
	[string]$RhythmRom,
	[string]$Output,
	[switch]$SkipNativeBuild,
	[switch]$SkipSmoke
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$worktreeRoot = [IO.Path]::GetFullPath((Join-Path $sourceRoot ".."))
$originalRoot = Join-Path $worktreeRoot "originals"
$standaloneRoot = [IO.Path]::GetFullPath(
	(Join-Path $sourceRoot "out\standalone")
)
if(-not $DataRoot) {
	$DataRoot = Join-Path $originalRoot "th03_process_root"
}
if(-not $MainExecutable) {
	$MainExecutable = Join-Path $originalRoot "th03_full\MAIN.EXE"
}
if(-not $MainlExecutable) {
	$MainlExecutable = Join-Path $originalRoot "th03_full\MAINL.EXE"
}
if(-not $OpExecutable) {
	$OpExecutable = Join-Path $originalRoot "th03_full\OP.EXE"
}
if(-not $FontRom) {
	$fontCandidates = @(
		(Join-Path $originalRoot "FONT.ROM"),
		(Join-Path $sourceRoot "out\classic-audio-binding-smoke\msvc\FONT.ROM"),
		(Join-Path $sourceRoot "out\classic-audio-binding-smoke\gcc\FONT.ROM")
	)
	$FontRom = $fontCandidates |
		Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
		Select-Object -First 1
	if(-not $FontRom) {
		$FontRom = $fontCandidates[0]
	}
}
if(-not $Output) {
	$Output = Join-Path $standaloneRoot "TH03-Classic-Test.exe"
}

function Resolve-RequiredFile([string]$Path, [string]$Label) {
	$resolved = [IO.Path]::GetFullPath($Path)
	if(-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
		throw "$Label does not exist: $resolved"
	}
	return $resolved
}

function Resolve-RequiredDirectory([string]$Path, [string]$Label) {
	$resolved = [IO.Path]::GetFullPath($Path)
	if(-not (Test-Path -LiteralPath $resolved -PathType Container)) {
		throw "$Label does not exist: $resolved"
	}
	return $resolved
}

$DataRoot = Resolve-RequiredDirectory $DataRoot "Classic data root"
$MainExecutable = Resolve-RequiredFile $MainExecutable "MAIN.EXE"
$MainlExecutable = Resolve-RequiredFile $MainlExecutable "MAINL.EXE"
$OpExecutable = Resolve-RequiredFile $OpExecutable "OP.EXE"
$FontRom = Resolve-RequiredFile $FontRom "FONT.ROM"
if($RhythmRom) {
	$RhythmRom = Resolve-RequiredFile $RhythmRom "YM2608 rhythm ROM"
}
foreach($archiveDirectory in @("YUME1.DAT", "YUME2.DAT")) {
	$null = Resolve-RequiredDirectory (
		Join-Path $DataRoot $archiveDirectory
	) "Extracted $archiveDirectory"
}

$buildRoot = Join-Path $sourceRoot "out\build\$BuildPreset"
if(-not $SkipNativeBuild) {
	& cmake --build --preset $BuildPreset --config $Configuration --target th03-app
	if($LASTEXITCODE -ne 0) {
		throw "Native th03-app build failed with exit code $LASTEXITCODE."
	}
}
$appCandidates = @(
	(Join-Path $buildRoot "$Configuration\th03-app.exe"),
	(Join-Path $buildRoot "th03-app.exe")
)
$nativeApp = $appCandidates |
	Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
	Select-Object -First 1
if(-not $nativeApp) {
	throw "The $BuildPreset build did not produce th03-app.exe."
}
$nativeApp = [IO.Path]::GetFullPath($nativeApp)
$sdlRuntime = Resolve-RequiredFile (
	Join-Path (Split-Path -Parent $nativeApp) "SDL3.dll"
) "SDL3 runtime"

[IO.Directory]::CreateDirectory($standaloneRoot) | Out-Null
$Output = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory((Split-Path -Parent $Output)) | Out-Null
$workRoot = Join-Path $standaloneRoot (
	"work-" + [Guid]::NewGuid().ToString("N")
)
$workRoot = [IO.Path]::GetFullPath($workRoot)
$standalonePrefix = $standaloneRoot.TrimEnd(
	[IO.Path]::DirectorySeparatorChar,
	[IO.Path]::AltDirectorySeparatorChar
) + [IO.Path]::DirectorySeparatorChar
if(-not $workRoot.StartsWith(
	$standalonePrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "Generated work directory escaped the standalone output root."
}

try {
	$payloadRoot = Join-Path $workRoot "payload"
	$appRoot = Join-Path $payloadRoot "app"
	$contentRoot = Join-Path $payloadRoot "content"
	$publishRoot = Join-Path $workRoot "publish"
	[IO.Directory]::CreateDirectory($appRoot) | Out-Null
	[IO.Directory]::CreateDirectory($contentRoot) | Out-Null
	[IO.Directory]::CreateDirectory($publishRoot) | Out-Null

	Copy-Item -LiteralPath $nativeApp -Destination $appRoot
	Copy-Item -LiteralPath $sdlRuntime -Destination $appRoot
	$licenseRoot = Join-Path (Split-Path -Parent $nativeApp) "licenses"
	if(Test-Path -LiteralPath $licenseRoot -PathType Container) {
		Copy-Item -LiteralPath $licenseRoot -Destination $appRoot -Recurse
	}
	foreach($archiveDirectory in @("YUME1.DAT", "YUME2.DAT")) {
		Copy-Item -LiteralPath (Join-Path $DataRoot $archiveDirectory) `
			-Destination $contentRoot -Recurse
	}
	Copy-Item -LiteralPath $MainExecutable `
		-Destination (Join-Path $contentRoot "MAIN.EXE")
	Copy-Item -LiteralPath $MainlExecutable `
		-Destination (Join-Path $contentRoot "MAINL.EXE")
	Copy-Item -LiteralPath $OpExecutable `
		-Destination (Join-Path $contentRoot "OP.EXE")
	Copy-Item -LiteralPath $FontRom `
		-Destination (Join-Path $contentRoot "FONT.ROM")
	if($RhythmRom) {
		Copy-Item -LiteralPath $RhythmRom `
			-Destination (Join-Path $contentRoot "YM2608.ROM")
	}
	[IO.File]::WriteAllText(
		(Join-Path $payloadRoot "LOCAL-TEST-BUILD.txt"),
		"Local testing artifact generated from caller-supplied content. " +
		"Do not redistribute before the public asset/license review.`r`n",
		[Text.Encoding]::UTF8
	)
	(Get-Item -LiteralPath (
		Join-Path $payloadRoot "LOCAL-TEST-BUILD.txt"
	)).LastWriteTimeUtc = [DateTime]::new(2000, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)

	if(-not $SkipSmoke) {
		$previousVideo = $env:SDL_VIDEODRIVER
		$previousAudio = $env:SDL_AUDIODRIVER
		try {
			$env:SDL_VIDEODRIVER = "dummy"
			$env:SDL_AUDIODRIVER = "dummy"
			$nativeOutput = @(& (Join-Path $appRoot "th03-app.exe") `
				--classic --data $contentRoot `
				--main (Join-Path $contentRoot "MAIN.EXE") `
				--mainl (Join-Path $contentRoot "MAINL.EXE") `
				--op (Join-Path $contentRoot "OP.EXE") `
				--font (Join-Path $contentRoot "FONT.ROM") `
				--config (Join-Path $workRoot "smoke-YUME.CFG") `
				--score (Join-Path $workRoot "smoke-YUME.NEM") `
				--smoke-ticks 8 2>&1)
			$nativeExit = $LASTEXITCODE
		} finally {
			$env:SDL_VIDEODRIVER = $previousVideo
			$env:SDL_AUDIODRIVER = $previousAudio
		}
		if($nativeExit -ne 0) {
			throw "Native payload smoke failed: $($nativeOutput -join ' ')"
		}
	}

	Add-Type -AssemblyName System.IO.Compression.FileSystem
	$payloadZip = Join-Path $workRoot "classic-payload.zip"
	[IO.Compression.ZipFile]::CreateFromDirectory(
		$payloadRoot,
		$payloadZip,
		[IO.Compression.CompressionLevel]::Optimal,
		$false
	)
	$launcherProject = Join-Path $PSScriptRoot (
		"classic-standalone\ClassicStandalone.csproj"
	)
	$publishArguments = @(
		"publish", $launcherProject,
		"-c", "Release",
		"-r", "win-x64",
		"--self-contained", "true",
		"-p:PayloadZip=$payloadZip",
		"-p:PublishDir=$publishRoot"
	)
	& dotnet @publishArguments
	if($LASTEXITCODE -ne 0) {
		throw "Standalone launcher publish failed with exit code $LASTEXITCODE."
	}
	$published = Resolve-RequiredFile (
		Join-Path $publishRoot "TH03-Classic-Test.exe"
	) "Published standalone executable"
	Copy-Item -LiteralPath $published -Destination $Output -Force

	if(-not $SkipSmoke) {
		$previousVideo = $env:SDL_VIDEODRIVER
		$previousAudio = $env:SDL_AUDIODRIVER
		$previousTicks = $env:TH03_CLASSIC_STANDALONE_SMOKE_TICKS
		$previousUserRoot = $env:TH03_CLASSIC_STANDALONE_USER_ROOT
		try {
			$env:SDL_VIDEODRIVER = "dummy"
			$env:SDL_AUDIODRIVER = "dummy"
			$env:TH03_CLASSIC_STANDALONE_SMOKE_TICKS = "8"
			$env:TH03_CLASSIC_STANDALONE_USER_ROOT = Join-Path $workRoot `
				"standalone-smoke-user"
			$smoke = Start-Process -FilePath $Output -Wait -PassThru `
				-WindowStyle Hidden
		} finally {
			$env:SDL_VIDEODRIVER = $previousVideo
			$env:SDL_AUDIODRIVER = $previousAudio
			$env:TH03_CLASSIC_STANDALONE_SMOKE_TICKS = $previousTicks
			$env:TH03_CLASSIC_STANDALONE_USER_ROOT = $previousUserRoot
		}
		if($smoke.ExitCode -ne 0) {
			throw "Standalone executable smoke failed with exit code $($smoke.ExitCode)."
		}
	}

	$result = Get-Item -LiteralPath $Output
	$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Output).Hash
	Write-Host "standalone=$($result.FullName)"
	Write-Host "bytes=$($result.Length)"
	Write-Host "sha256=$hash"
} finally {
	if(Test-Path -LiteralPath $workRoot -PathType Container) {
		Remove-Item -LiteralPath $workRoot -Recurse -Force
	}
}
