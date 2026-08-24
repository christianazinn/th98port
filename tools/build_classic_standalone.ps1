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
	[string]$StartupLogo,
	[string]$LanguageOverlay,
	[string]$PicturePatch,
	[string]$ReplayPatchAssets,
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
$standaloneRoot = [IO.Path]::GetFullPath(
	(Join-Path $sourceRoot "out\standalone")
)

function Find-AncestorRelativePath(
	[string]$Start,
	[string]$RelativePath,
	[string]$PathType
) {
	$directory = [IO.DirectoryInfo]::new([IO.Path]::GetFullPath($Start))
	while($null -ne $directory) {
		$candidate = Join-Path $directory.FullName $RelativePath
		if(Test-Path -LiteralPath $candidate -PathType $PathType) {
			return $candidate
		}
		$directory = $directory.Parent
	}
	return $null
}

function Find-AncestorRelativeFile([string]$Start, [string]$RelativePath) {
	return Find-AncestorRelativePath $Start $RelativePath "Leaf"
}

function Find-AncestorRelativeDirectory([string]$Start, [string]$RelativePath) {
	return Find-AncestorRelativePath $Start $RelativePath "Container"
}

$originalRoot = Find-AncestorRelativeDirectory $worktreeRoot "originals"
if(-not $originalRoot) {
	$originalRoot = Join-Path $worktreeRoot "originals"
}
$canonicalSourceRoot = Join-Path (Split-Path -Parent $originalRoot) "th03-port"
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
	$siblingMsvcFont = Find-AncestorRelativeFile $worktreeRoot (
		"th03-v12-normalizer\th03-port\out\classic-audio-binding-smoke\msvc\FONT.ROM"
	)
	$siblingGccFont = Find-AncestorRelativeFile $worktreeRoot (
		"th03-v12-normalizer\th03-port\out\classic-audio-binding-smoke\gcc\FONT.ROM"
	)
	$fontCandidates = @(
		(Join-Path $originalRoot "FONT.ROM"),
		(Join-Path $sourceRoot "out\classic-audio-binding-smoke\msvc\FONT.ROM"),
		(Join-Path $sourceRoot "out\classic-audio-binding-smoke\gcc\FONT.ROM"),
		(Join-Path $canonicalSourceRoot "out\classic-audio-binding-smoke\msvc\FONT.ROM"),
		(Join-Path $canonicalSourceRoot "out\classic-audio-binding-smoke\gcc\FONT.ROM"),
		$siblingMsvcFont,
		$siblingGccFont
	)
	$FontRom = $fontCandidates |
		Where-Object {
			-not [string]::IsNullOrWhiteSpace($_) -and
			(Test-Path -LiteralPath $_ -PathType Leaf)
		} |
		Select-Object -First 1
	if(-not $FontRom) {
		$FontRom = $fontCandidates[0]
	}
}
if(-not $StartupLogo) {
	$StartupLogo = Join-Path $originalRoot "th03_full\TOUHOU.DAT"
}
if(-not $LanguageOverlay) {
	$siblingLanguageOverlay = Find-AncestorRelativeFile $worktreeRoot (
		"replays-and-validation-17ptz\.scratch\assets\th03-language\TH3EN.DAT"
	)
	$languageCandidates = @(
		(Join-Path $originalRoot "th03_en\TH3EN.DAT"),
		$siblingLanguageOverlay
	)
	$LanguageOverlay = $languageCandidates |
		Where-Object {
			-not [string]::IsNullOrWhiteSpace($_) -and
			(Test-Path -LiteralPath $_ -PathType Leaf)
		} |
		Select-Object -First 1
	if(-not $LanguageOverlay) {
		throw "The maintained standalone requires authenticated TH3EN.DAT bilingual content."
	}
}
if(-not $ReplayPatchAssets) {
	$siblingReplayPatchAssets = Find-AncestorRelativeFile $worktreeRoot (
		"replays-and-validation-17ptz\.scratch\assets\th03-replay-ui\AZINN.DAT"
	)
	$assetCandidates = @(
		(Join-Path $worktreeRoot ".scratch\assets\th03-replay-ui\AZINN.DAT"),
		(Join-Path $originalRoot "th03_full\AZINN.DAT"),
		$siblingReplayPatchAssets
	)
	$ReplayPatchAssets = $assetCandidates |
		Where-Object {
			-not [string]::IsNullOrWhiteSpace($_) -and
			(Test-Path -LiteralPath $_ -PathType Leaf)
		} |
		Select-Object -First 1
	if(-not $ReplayPatchAssets) {
		$ReplayPatchAssets = $assetCandidates[0]
	}
}
$usingDefaultOutput = [string]::IsNullOrWhiteSpace($Output)
if($usingDefaultOutput) {
	$Output = Join-Path $standaloneRoot "PoDD-Port-Test.exe"
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

function Publish-SmokeFailure(
	[string]$Label,
	[int]$ExitCode,
	[string]$UserRoot,
	[string]$StandardOutput,
	[string]$StandardError,
	[string]$Destination
) {
	$parts = [Collections.Generic.List[string]]::new()
	$parts.Add("smoke=$Label`r`nexit-code=$ExitCode`r`n")
	foreach($path in @(
		(Join-Path $UserRoot "last-run.log"),
		$StandardOutput,
		$StandardError
	)) {
		if([IO.File]::Exists($path)) {
			$parts.Add([IO.File]::ReadAllText($path))
		}
	}
	[IO.File]::WriteAllText(
		$Destination,
		($parts -join "`r`n"),
		[Text.UTF8Encoding]::new($false)
	)
}

$DataRoot = Resolve-RequiredDirectory $DataRoot "TH03 data root"
$MainExecutable = Resolve-RequiredFile $MainExecutable "MAIN.EXE"
$MainlExecutable = Resolve-RequiredFile $MainlExecutable "MAINL.EXE"
$OpExecutable = Resolve-RequiredFile $OpExecutable "OP.EXE"
$FontRom = Resolve-RequiredFile $FontRom "FONT.ROM"
$StartupLogo = Resolve-RequiredFile $StartupLogo "TOUHOU.DAT startup logo"
$ReplayPatchAssets = Resolve-RequiredFile $ReplayPatchAssets "AZINN.DAT"
$observedStartupLogoHash = (Get-FileHash -Algorithm SHA256 `
	-LiteralPath $StartupLogo).Hash
$expectedStartupLogoHash =
	"A9C31CF058B528479FE9C5B8FB7A4AC538000C8C2F3B9FB276CBC3DD7AA20532"
if($observedStartupLogoHash -ne $expectedStartupLogoHash) {
	throw "TOUHOU.DAT is not the authenticated TH01-TH03 ZUN Soft logo profile."
}
if($LanguageOverlay) {
	$LanguageOverlay = Resolve-RequiredFile $LanguageOverlay "TH3EN.DAT"
}
if($PicturePatch) {
	$PicturePatch = Resolve-RequiredFile $PicturePatch "T3PTP001 picture patch"
}
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
	$nativeBuildExit = 0
	Push-Location $sourceRoot
	try {
		& cmake --build --preset $BuildPreset --config $Configuration `
			--target th03-app podd-bootstrap
		$nativeBuildExit = $LASTEXITCODE
	} finally {
		Pop-Location
	}
	if($nativeBuildExit -ne 0) {
		throw "Native th03-app build failed with exit code $nativeBuildExit."
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
$bootstrapCandidates = @(
	(Join-Path $buildRoot "$Configuration\podd-bootstrap.exe"),
	(Join-Path $buildRoot "podd-bootstrap.exe")
)
$nativeBootstrap = $bootstrapCandidates |
	Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
	Select-Object -First 1
if(-not $nativeBootstrap) {
	throw "The $BuildPreset build did not produce podd-bootstrap.exe."
}
$nativeBootstrap = [IO.Path]::GetFullPath($nativeBootstrap)
$sdlRuntime = Resolve-RequiredFile (
	Join-Path (Split-Path -Parent $nativeApp) "SDL3.dll"
) "SDL3 runtime"

[IO.Directory]::CreateDirectory($standaloneRoot) | Out-Null
$failedSmokeLog = Join-Path $standaloneRoot "last-failed-smoke.log"
if([IO.File]::Exists($failedSmokeLog)) {
	Remove-Item -LiteralPath $failedSmokeLog -Force
}
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
$temporaryPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd(
	[IO.Path]::DirectorySeparatorChar,
	[IO.Path]::AltDirectorySeparatorChar
) + [IO.Path]::DirectorySeparatorChar
$smokeProductRoot = [IO.Path]::GetFullPath((Join-Path $temporaryPrefix (
	"th03-podd-port-smoke-" + [Guid]::NewGuid().ToString("N")
)))
if(-not $smokeProductRoot.StartsWith(
	$temporaryPrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "Generated smoke directory escaped the system temporary root."
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
	Copy-Item -LiteralPath $nativeBootstrap -Destination $appRoot
	Copy-Item -LiteralPath $sdlRuntime -Destination $appRoot
	$licenseRoot = Join-Path (Split-Path -Parent $nativeApp) "licenses"
	if(Test-Path -LiteralPath $licenseRoot -PathType Container) {
		Copy-Item -LiteralPath $licenseRoot -Destination $appRoot -Recurse
	}
	$unifontLicense = Resolve-RequiredFile (
		Join-Path $sourceRoot "third_party\unifont\OFL-1.1.txt"
	) "GNU Unifont OFL license"
	$unifontLicenseDestination = Join-Path $appRoot "licenses\GNU-Unifont"
	[IO.Directory]::CreateDirectory($unifontLicenseDestination) | Out-Null
	Copy-Item -LiteralPath $unifontLicense `
		-Destination (Join-Path $unifontLicenseDestination "OFL-1.1.txt")
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
	Copy-Item -LiteralPath $StartupLogo `
		-Destination (Join-Path $contentRoot "TOUHOU.DAT")
	Copy-Item -LiteralPath $ReplayPatchAssets `
		-Destination (Join-Path $contentRoot "AZINN.DAT")
	$menuAssetSource = Join-Path $sourceRoot (
		"assets\new-classic\menu-backgrounds"
	)
	$menuAssetFiles = Get-ChildItem -LiteralPath $menuAssetSource `
		-Filter "*.BMP" -File
	if($menuAssetFiles.Count -ne 16) {
		throw "Expected exactly 16 repo-owned New Classic menu background halves."
	}
	# th03-app binds repo-owned UI assets relative to its executable. Keep them
	# in app/PODDUI in both the payload smoke tree and bootstrap extraction.
	$menuAssetDestination = Join-Path $appRoot "PODDUI"
	[IO.Directory]::CreateDirectory($menuAssetDestination) | Out-Null
	foreach($menuAsset in $menuAssetFiles) {
		Copy-Item -LiteralPath $menuAsset.FullName `
			-Destination $menuAssetDestination
	}
	$unicodeFont = Join-Path $menuAssetSource "T3UFN001.BIN"
	if(!(Test-Path -LiteralPath $unicodeFont -PathType Leaf)) {
		throw "Missing deterministic T3UFN001 Unicode font package."
	}
	Copy-Item -LiteralPath $unicodeFont -Destination $menuAssetDestination
	if($LanguageOverlay) {
		Copy-Item -LiteralPath $LanguageOverlay `
			-Destination (Join-Path $contentRoot "TH3EN.DAT")
	}
	if($PicturePatch) {
		Copy-Item -LiteralPath $PicturePatch `
			-Destination (Join-Path $contentRoot "picture-patch-en-US.t3ptp")
	}
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
				"--shell", "--data", $contentRoot,
				"--main", (Join-Path $contentRoot "MAIN.EXE"),
				"--mainl", (Join-Path $contentRoot "MAINL.EXE"),
				"--op", (Join-Path $contentRoot "OP.EXE"),
				"--font", (Join-Path $contentRoot "FONT.ROM"),
				"--config", (Join-Path $workRoot "smoke-YUME.CFG"),
				"--score", (Join-Path $workRoot "smoke-YUME.NEM"),
				"--smoke-ticks", "8"
			)
			if($LanguageOverlay) {
				$nativeArguments += @(
					"--language-overlay", (Join-Path $contentRoot "TH3EN.DAT")
				)
			}
			if($PicturePatch) {
				$nativeArguments += @(
					"--picture-patch-pack", (Join-Path $contentRoot (
						"picture-patch-en-US.t3ptp"
					))
				)
			}
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

	# The resource loader authenticates and opens extracted members directly.
	# These packed-archive copies and unused BF2 have no manifest/source consumer;
	# omitting them changes no path reachable by the native application.
	foreach($archiveName in @("YUME1.DAT", "YUME2.DAT")) {
		$archiveRoot = Join-Path $contentRoot $archiveName
		foreach($unusedName in @("yume1.DAT", "yume2.DAT", "NUM8.BF2")) {
			$unusedPath = Join-Path $archiveRoot $unusedName
			if([IO.File]::Exists($unusedPath)) {
				Remove-Item -LiteralPath $unusedPath -Force
			}
		}
	}

	$archiveOne = Join-Path $contentRoot "YUME1.DAT"
	$archiveTwo = Join-Path $contentRoot "YUME2.DAT"
	$archiveOneFiles = Get-ChildItem -LiteralPath $archiveOne -Recurse -File
	$archiveTwoFiles = Get-ChildItem -LiteralPath $archiveTwo -Recurse -File
	$archiveOneManifest = @{}
	foreach($file in $archiveOneFiles) {
		$relative = [IO.Path]::GetRelativePath($archiveOne, $file.FullName)
		$archiveOneManifest[$relative] = @(
			$file.Length,
			(Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
		)
	}
	$archivesIdentical = $archiveOneFiles.Count -eq $archiveTwoFiles.Count
	if($archivesIdentical) {
		foreach($file in $archiveTwoFiles) {
			$relative = [IO.Path]::GetRelativePath($archiveTwo, $file.FullName)
			if(-not $archiveOneManifest.ContainsKey($relative)) {
				$archivesIdentical = $false
				break
			}
			$expected = $archiveOneManifest[$relative]
			$observedHash = (
				Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName
			).Hash
			if($file.Length -ne $expected[0] -or $observedHash -ne $expected[1]) {
				$archivesIdentical = $false
				break
			}
		}
	}
	if($archivesIdentical) {
		# [archiveTwo] is inside the already validated generated work root.
		# The NativeAOT launcher recreates it as per-file hard links, with a
		# byte-copy fallback when the destination filesystem lacks hard links.
		Remove-Item -LiteralPath $archiveTwo -Recurse -Force
		$dedupMarker = Join-Path $payloadRoot ".deduplicate-YUME2.DAT"
		[IO.File]::WriteAllText(
			$dedupMarker,
			"YUME1.DAT -> YUME2.DAT`r`n",
			[Text.Encoding]::ASCII
		)
		(Get-Item -LiteralPath $dedupMarker).LastWriteTimeUtc =
			[DateTime]::new(2000, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
	}
	$payloadEpoch = [DateTime]::new(
		2000, 1, 1, 0, 0, 0, [DateTimeKind]::Utc
	)
	Get-ChildItem -LiteralPath $payloadRoot -Recurse -Force |
		ForEach-Object { $_.LastWriteTimeUtc = $payloadEpoch }

	Add-Type -AssemblyName System.IO.Compression.FileSystem
	$payloadZip = Join-Path $workRoot "podd-port-payload.zip"
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
		Join-Path $publishRoot "PoDD-Port-Test.exe"
	) "Published standalone executable"
	Copy-Item -LiteralPath $published -Destination $Output -Force

	if(-not $SkipSmoke) {
		$previousVideo = $env:SDL_VIDEODRIVER
		$previousAudio = $env:SDL_AUDIODRIVER
		$previousProductRoot = $env:TH03_PODD_PORT_PRODUCT_ROOT
		$previousLegacyProductRoot = $env:TH03_PODD_PORT_LEGACY_PRODUCT_ROOT
		$previousTicks = $env:TH03_PODD_PORT_SMOKE_TICKS
		$previousUserRoot = $env:TH03_PODD_PORT_USER_ROOT
		$previousReplayRoot = $env:TH03_PODD_PORT_REPLAY_ROOT
		try {
			$env:SDL_VIDEODRIVER = "dummy"
			$env:SDL_AUDIODRIVER = "dummy"
			$env:TH03_PODD_PORT_SMOKE_TICKS = "8"
			Remove-Item Env:TH03_PODD_PORT_REPLAY_ROOT `
				-ErrorAction SilentlyContinue

			$portableRoot = Join-Path $workRoot "portable-smoke"
			$portableProduct = Join-Path $smokeProductRoot "portable-product"
			$portableLegacyProduct = Join-Path $smokeProductRoot (
				"portable-smoke-legacy-product"
			)
			$portableUser = Join-Path $portableProduct "user"
			[IO.Directory]::CreateDirectory($portableRoot) | Out-Null
			[IO.Directory]::CreateDirectory(
				(Join-Path $portableLegacyProduct "user")
			) | Out-Null
			$migrationMarker = Join-Path $portableLegacyProduct (
				"user\profile-migration.marker"
			)
			[IO.File]::WriteAllText($migrationMarker, "PoDD Port")
			$portableExecutable = Join-Path $portableRoot "PoDD-Port-Test.exe"
			Copy-Item -LiteralPath $Output -Destination $portableExecutable
			$portableOut = Join-Path $workRoot "portable-smoke.stdout.log"
			$portableErr = Join-Path $workRoot "portable-smoke.stderr.log"
			$env:TH03_PODD_PORT_PRODUCT_ROOT = $portableProduct
			$env:TH03_PODD_PORT_LEGACY_PRODUCT_ROOT = $portableLegacyProduct
			$env:TH03_PODD_PORT_USER_ROOT = $portableUser
			$smoke = Start-Process -FilePath $portableExecutable -Wait -PassThru `
				-WindowStyle Hidden -RedirectStandardOutput $portableOut `
				-RedirectStandardError $portableErr
			if($smoke.ExitCode -ne 0) {
				Publish-SmokeFailure "portable" $smoke.ExitCode $portableUser `
					$portableOut $portableErr $failedSmokeLog
				throw "Standalone portable smoke failed with exit code $($smoke.ExitCode). See $failedSmokeLog"
			}
			if([IO.Directory]::Exists($portableLegacyProduct) -or
				-not [IO.File]::Exists((Join-Path $portableUser (
					"profile-migration.marker"
				)))) {
				throw "Standalone AppData profile migration smoke failed."
			}
			$portableLog = Join-Path $portableUser "last-run.log"
			$portableReplay = Join-Path $portableRoot "REPLAY"
			if(-not [IO.Directory]::Exists($portableReplay) -or
				-not ([IO.File]::ReadAllText($portableLog).Contains(
					"replay-root=$portableReplay"
				))) {
				throw "Standalone portable replay-root smoke failed."
			}

			$installLine = [IO.File]::ReadAllLines($portableLog) |
				Where-Object { $_.StartsWith("install-root=") } |
				Select-Object -First 1
			if(-not $installLine) {
				throw "Standalone smoke did not report its extracted install root."
			}
			$installRoot = $installLine.Substring("install-root=".Length)
			$treeOne = Join-Path $installRoot "content\YUME1.DAT"
			$treeTwo = Join-Path $installRoot "content\YUME2.DAT"
			$oneFiles = Get-ChildItem -LiteralPath $treeOne -File -Recurse
			$twoFiles = Get-ChildItem -LiteralPath $treeTwo -File -Recurse
			if($oneFiles.Count -ne $twoFiles.Count) {
				throw "Standalone deduplicated archive restore count differs."
			}
			foreach($file in $oneFiles) {
				$relative = [IO.Path]::GetRelativePath($treeOne, $file.FullName)
				$other = Join-Path $treeTwo $relative
				if(-not [IO.File]::Exists($other) -or
					$file.Length -ne (Get-Item -LiteralPath $other).Length -or
					(Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash -ne
						(Get-FileHash -Algorithm SHA256 -LiteralPath $other).Hash) {
					throw "Standalone deduplicated archive restore differs at $relative."
				}
			}

			$fallbackRoot = Join-Path $workRoot "fallback-smoke"
			$fallbackUser = Join-Path $workRoot "fallback-smoke-user"
			[IO.Directory]::CreateDirectory($fallbackRoot) | Out-Null
			$fallbackExecutable = Join-Path $fallbackRoot "PoDD-Port-Test.exe"
			Copy-Item -LiteralPath $Output -Destination $fallbackExecutable
			[IO.File]::WriteAllText((Join-Path $fallbackRoot "REPLAY"), "blocked")
			$fallbackOut = Join-Path $workRoot "fallback-smoke.stdout.log"
			$fallbackErr = Join-Path $workRoot "fallback-smoke.stderr.log"
			$env:TH03_PODD_PORT_PRODUCT_ROOT = Join-Path $smokeProductRoot (
				"fallback-smoke-product"
			)
			Remove-Item Env:TH03_PODD_PORT_LEGACY_PRODUCT_ROOT `
				-ErrorAction SilentlyContinue
			$env:TH03_PODD_PORT_USER_ROOT = $fallbackUser
			$smoke = Start-Process -FilePath $fallbackExecutable -Wait -PassThru `
				-WindowStyle Hidden -RedirectStandardOutput $fallbackOut `
				-RedirectStandardError $fallbackErr
			if($smoke.ExitCode -ne 0) {
				Publish-SmokeFailure "fallback" $smoke.ExitCode $fallbackUser `
					$fallbackOut $fallbackErr $failedSmokeLog
				throw "Standalone fallback smoke failed with exit code $($smoke.ExitCode). See $failedSmokeLog"
			}
			$fallbackLog = Join-Path $fallbackUser "last-run.log"
			$fallbackReplay = Join-Path $fallbackUser "REPLAY"
			if(-not [IO.Directory]::Exists($fallbackReplay) -or
				-not ([IO.File]::ReadAllText($fallbackLog).Contains(
					"replay-root=$fallbackReplay"
				))) {
				throw "Standalone private replay-root fallback smoke failed."
			}
		} finally {
			$env:SDL_VIDEODRIVER = $previousVideo
			$env:SDL_AUDIODRIVER = $previousAudio
			$env:TH03_PODD_PORT_PRODUCT_ROOT = $previousProductRoot
			$env:TH03_PODD_PORT_LEGACY_PRODUCT_ROOT = $previousLegacyProductRoot
			$env:TH03_PODD_PORT_SMOKE_TICKS = $previousTicks
			$env:TH03_PODD_PORT_USER_ROOT = $previousUserRoot
			$env:TH03_PODD_PORT_REPLAY_ROOT = $previousReplayRoot
		}
	}
	if($usingDefaultOutput) {
		$legacyOutput = Join-Path $standaloneRoot "TH03-Classic-Test.exe"
		if([IO.File]::Exists($legacyOutput)) {
			Remove-Item -LiteralPath $legacyOutput -Force
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
	if(Test-Path -LiteralPath $smokeProductRoot -PathType Container) {
		Remove-Item -LiteralPath $smokeProductRoot -Recurse -Force
	}
}
