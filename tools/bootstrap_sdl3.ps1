param(
	[string]$DestinationRoot = "",
	[Alias("Archive")]
	[string]$ArchivePath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$version = "3.4.10"
$archiveName = "SDL3-$version.tar.gz"
$expectedSha256 =
	"12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785"
$publishedUrl =
	"https://github.com/libsdl-org/SDL/releases/download/release-$version/$archiveName"
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $sourceRoot "out"))
if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
	$DestinationRoot = Join-Path $allowedRoot "deps"
}
$destination = [IO.Path]::GetFullPath($DestinationRoot)
$allowedPrefix = $allowedRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) +
	[IO.Path]::DirectorySeparatorChar
if (-not $destination.StartsWith(
	$allowedPrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "SDL destination must remain below th03-port/out"
}

if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
	throw (
		"Offline bootstrap requires -ArchivePath pointing to the explicitly " +
		"acquired release asset: $publishedUrl"
	)
}
$archive = [IO.Path]::GetFullPath($ArchivePath)
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
	throw "SDL release archive does not exist: $archive"
}

# Authentication happens before the script creates or changes any destination.
$actualSha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
if ($actualSha256 -ne $expectedSha256) {
	throw "SDL archive SHA-256 mismatch"
}

$archiveEntries = @(& tar.exe -tzf $archive)
if ($LASTEXITCODE -ne 0 -or $archiveEntries.Count -eq 0) {
	throw "SDL archive inspection failed"
}
$expectedArchivePrefix = "SDL3-$version/"
foreach ($entry in $archiveEntries) {
	if (-not $entry.StartsWith(
		$expectedArchivePrefix,
		[StringComparison]::Ordinal
	)) {
		throw "SDL archive contained an unexpected top-level layout"
	}
	$relativeEntry = $entry.Substring($expectedArchivePrefix.Length)
	if ($relativeEntry.Contains("\") -or $relativeEntry.Contains(":")) {
		throw "SDL archive contained an unsafe path"
	}
	foreach ($segment in $relativeEntry.Split("/")) {
		if ($segment -eq "." -or $segment -eq "..") {
			throw "SDL archive contained an unsafe path"
		}
	}
}

$source = Join-Path $destination "SDL3-$version"
$markerName = ".th03-source-sha256"

function Assert-SdlSource {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Path,
		[Parameter(Mandatory = $true)]
		[bool]$RequireMarker
	)

	$cmakeFile = Join-Path $Path "CMakeLists.txt"
	$versionHeader = Join-Path $Path "include/SDL3/SDL_version.h"
	$licenseFile = Join-Path $Path "LICENSE.txt"
	foreach ($requiredFile in @($cmakeFile, $versionHeader, $licenseFile)) {
		if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
			throw "SDL source directory is incomplete: $requiredFile"
		}
	}
	if ((Get-Item -LiteralPath $licenseFile).Length -eq 0) {
		throw "SDL LICENSE.txt is empty"
	}

	$versionText = [IO.File]::ReadAllText($versionHeader)
	$versionPatterns = @(
		"(?m)^[\t ]*#[\t ]*define[\t ]+SDL_MAJOR_VERSION[\t ]+3([^0-9]|`$)",
		"(?m)^[\t ]*#[\t ]*define[\t ]+SDL_MINOR_VERSION[\t ]+4([^0-9]|`$)",
		"(?m)^[\t ]*#[\t ]*define[\t ]+SDL_MICRO_VERSION[\t ]+10([^0-9]|`$)"
	)
	foreach ($pattern in $versionPatterns) {
		if (-not [Text.RegularExpressions.Regex]::IsMatch(
			$versionText,
			$pattern
		)) {
			throw "SDL source version header is not exactly $version"
		}
	}

	if ($RequireMarker) {
		$marker = Join-Path $Path $markerName
		if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
			throw "Existing SDL source directory lacks the authenticated marker"
		}
		if ([IO.File]::ReadAllText($marker).Trim() -ne $expectedSha256) {
			throw "Existing SDL source marker does not match the release archive"
		}
	}
}

if (Test-Path -LiteralPath $source) {
	if (-not (Test-Path -LiteralPath $source -PathType Container)) {
		throw "SDL source destination exists but is not a directory"
	}
	Assert-SdlSource -Path $source -RequireMarker $true
	Write-Output $source
	exit 0
}

$destinationExisted = Test-Path -LiteralPath $destination
if ($destinationExisted -and
	-not (Test-Path -LiteralPath $destination -PathType Container)
) {
	throw "SDL destination root exists but is not a directory"
}
if (-not $destinationExisted) {
	New-Item -ItemType Directory -Path $destination | Out-Null
}

$stagingName = ".SDL3-$version.extract-$([Guid]::NewGuid().ToString('N'))"
$staging = Join-Path $destination $stagingName
$destinationPrefix = $destination.TrimEnd([IO.Path]::DirectorySeparatorChar) +
	[IO.Path]::DirectorySeparatorChar
$resolvedStaging = [IO.Path]::GetFullPath($staging)
if (-not $resolvedStaging.StartsWith(
	$destinationPrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "Internal SDL staging path escaped the destination root"
}

$published = $false
try {
	New-Item -ItemType Directory -Path $resolvedStaging | Out-Null
	& tar.exe -xzf $archive -C $resolvedStaging --strip-components=1
	if ($LASTEXITCODE -ne 0) {
		throw "SDL archive extraction failed"
	}
	Assert-SdlSource -Path $resolvedStaging -RequireMarker $false

	# The marker is written only after the complete extracted tree validates.
	$markerPath = Join-Path $resolvedStaging $markerName
	$utf8WithoutBom = New-Object Text.UTF8Encoding($false)
	[IO.File]::WriteAllText($markerPath, $expectedSha256, $utf8WithoutBom)
	Move-Item -LiteralPath $resolvedStaging -Destination $source
	$published = $true
} finally {
	if (-not $published -and
		(Test-Path -LiteralPath $resolvedStaging -PathType Container)
	) {
		$cleanupPath = [IO.Path]::GetFullPath($resolvedStaging)
		if (-not $cleanupPath.StartsWith(
			$destinationPrefix,
			[StringComparison]::OrdinalIgnoreCase
		)) {
			throw "Refusing to clean an SDL staging path outside the destination"
		}
		Remove-Item -LiteralPath $cleanupPath -Recurse -Force
	}
	if (-not $destinationExisted -and
		(Test-Path -LiteralPath $destination -PathType Container) -and
		@((Get-ChildItem -Force -LiteralPath $destination)).Count -eq 0
	) {
		Remove-Item -LiteralPath $destination
	}
}

Assert-SdlSource -Path $source -RequireMarker $true
Write-Output $source
