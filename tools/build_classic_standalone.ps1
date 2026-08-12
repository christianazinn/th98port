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
	[string]$T98Sound86,
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
if(-not $RhythmRom -and -not $T98Sound86) {
	$t98Candidate = Join-Path $env:USERPROFILE (
		"Games\2hu\PC-98\T98 english\T98 english\DEVICE\Sound86.nhw"
	)
	if(Test-Path -LiteralPath $t98Candidate -PathType Leaf) {
		$T98Sound86 = $t98Candidate
	}
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
if($T98Sound86) {
	$T98Sound86 = Resolve-RequiredFile $T98Sound86 "T98-Next Sound86 device"
}
if($RhythmRom -and $T98Sound86) {
	throw "Specify either RhythmRom or T98Sound86, not both."
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
	if($T98Sound86) {
		# The installed and official-update modules have identical executable
		# and rhythm-data sections; their resource sections differ. Authenticate
		# each observed full-file profile here, then independently authenticate
		# the extracted six-voice payload below.
		$expectedSound86Hashes = @(
			"1DB3840C09908BA859CDFABC43C59EA89FDAA7F02DC91A8E952BAF369412FF88",
			"068882F89CD1E187878FE7052B19D02325D100FA8CAECBB63EBCB0FD96518916",
			"81D0C3071A0B242C6649308B25C277E5413E2214AEA7CDFCFB62DE5BDC651F01"
		)
		$observedSound86Hash = (Get-FileHash -Algorithm SHA256 `
			-LiteralPath $T98Sound86).Hash
		if($observedSound86Hash -notin $expectedSound86Hashes) {
			throw "T98-Next Sound86.nhw digest is not an accepted Sound86 profile."
		}
		$device = [IO.File]::ReadAllBytes($T98Sound86)
		$voiceOffsets = @(0x120F0, 0x22840, 0x2FD1C, 0x16C10, 0x29E60, 0x1E810)
		$voiceLengths = @(0x2590, 0x3B10, 0x6EED, 0x3E00, 0x2F5E, 0x2018)
		$stream = [IO.MemoryStream]::new()
		$writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::UTF8, $true)
		try {
			$writer.Write([byte[]](0x54, 0x39, 0x38, 0x52, 0x48, 0x59, 0x30, 0x00))
			$writer.Write([uint32]44100)
			foreach($length in $voiceLengths) {
				$writer.Write([uint32]$length)
			}
			for($voice = 0; $voice -lt $voiceOffsets.Count; ++$voice) {
				$writer.Write(
					$device,
					$voiceOffsets[$voice],
					$voiceLengths[$voice] * 2
				)
			}
			$writer.Flush()
			$rhythmPcm = $stream.ToArray()
		} finally {
			$writer.Dispose()
			$stream.Dispose()
		}
		$observedPcmHash = [Convert]::ToHexString(
			[Security.Cryptography.SHA256]::HashData($rhythmPcm)
		)
		$expectedPcmHash =
			"225FD07B20C51AC1438E426F92252C41ACE95F039CA52A59A30496169D0C8953"
		if($observedPcmHash -ne $expectedPcmHash) {
			throw "Extracted T98 rhythm profile failed its deterministic digest."
		}
		$rhythmPcmPath = Join-Path $contentRoot "T98RHYTHM.PCM"
		[IO.File]::WriteAllBytes($rhythmPcmPath, $rhythmPcm)
		(Get-Item -LiteralPath $rhythmPcmPath).LastWriteTimeUtc =
			[DateTime]::new(2000, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
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
			$nativeArguments = @(
				"--classic", "--data", $contentRoot,
				"--main", (Join-Path $contentRoot "MAIN.EXE"),
				"--mainl", (Join-Path $contentRoot "MAINL.EXE"),
				"--op", (Join-Path $contentRoot "OP.EXE"),
				"--font", (Join-Path $contentRoot "FONT.ROM"),
				"--config", (Join-Path $workRoot "smoke-YUME.CFG"),
				"--score", (Join-Path $workRoot "smoke-YUME.NEM"),
				"--smoke-ticks", "8"
			)
			if($RhythmRom) {
				$nativeArguments += @(
					"--rhythm-rom", (Join-Path $contentRoot "YM2608.ROM")
				)
			}
			if($T98Sound86) {
				$nativeArguments += @(
					"--rhythm-pcm", (Join-Path $contentRoot "T98RHYTHM.PCM")
				)
			}
			$nativeOutput = @(& (Join-Path $appRoot "th03-app.exe") `
				@nativeArguments 2>&1)
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
