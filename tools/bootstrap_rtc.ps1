param(
	[Parameter(Mandatory = $true)]
	[string]$LibDataChannelSource,
	[Parameter(Mandatory = $true)]
	[string]$MbedTlsArchive,
	[string]$DestinationRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

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
	throw "RTC dependency destination must remain below th03-port/out"
}

$libVersion = "0.24.3"
$libCommit = "c6696d157b5612df2a741d9a03b192b47ab6cefb"
$submodules = [ordered]@{
	"deps/json" = "55f93686c01528224f448c19128836e7df245f72"
	"deps/libjuice" = "22145ec3473c73d811417ac9a6aee0de530aaa3f"
	"deps/libsrtp" = "24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5"
	"deps/plog" = "94899e0b926ac1b0f4750bfbd495167b4a6ae9ef"
	"deps/usrsctp" = "fec583d54493f879d2ae44a743423bf8a04371ab"
}
$mbedVersion = "3.6.7"
$mbedSha256 =
	"a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6"

$libSource = [IO.Path]::GetFullPath($LibDataChannelSource)
if (-not (Test-Path -LiteralPath $libSource -PathType Container)) {
	throw "libdatachannel source directory does not exist: $libSource"
}
$head = (& git.exe -C $libSource rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $libCommit) {
	throw "libdatachannel source is not the pinned v$libVersion commit"
}
$dirty = @(& git.exe -C $libSource status --porcelain --ignore-submodules=none)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
	throw "libdatachannel source and recursive submodules must be clean"
}
foreach ($entry in $submodules.GetEnumerator()) {
	$actual = (& git.exe -C $libSource -C $entry.Key rev-parse HEAD).Trim()
	if ($LASTEXITCODE -ne 0 -or $actual -ne $entry.Value) {
		throw "libdatachannel submodule pin mismatch: $($entry.Key)"
	}
}
foreach ($required in @(
	"CMakeLists.txt", "LICENSE", "include/rtc/rtc.hpp",
	"deps/libjuice/LICENSE", "deps/usrsctp/LICENSE.md",
	"deps/json/LICENSE.MIT", "deps/plog/LICENSE"
)) {
	if (-not (Test-Path -LiteralPath (Join-Path $libSource $required) -PathType Leaf)) {
		throw "libdatachannel source is incomplete: $required"
	}
}

$archive = [IO.Path]::GetFullPath($MbedTlsArchive)
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
	throw "Mbed TLS release archive does not exist: $archive"
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne
	$mbedSha256) {
	throw "Mbed TLS archive SHA-256 mismatch"
}
$archiveEntries = @(& tar.exe -tf $archive)
if ($LASTEXITCODE -ne 0 -or $archiveEntries.Count -eq 0) {
	throw "Mbed TLS archive inspection failed"
}
$archivePrefix = "mbedtls-$mbedVersion/"
foreach ($entry in $archiveEntries) {
	if (-not $entry.StartsWith($archivePrefix, [StringComparison]::Ordinal)) {
		throw "Mbed TLS archive contained an unexpected top-level layout"
	}
	$relative = $entry.Substring($archivePrefix.Length)
	if ($relative.Contains("\") -or $relative.Contains(":")) {
		throw "Mbed TLS archive contained an unsafe path"
	}
	foreach ($segment in $relative.Split("/")) {
		if ($segment -eq "." -or $segment -eq "..") {
			throw "Mbed TLS archive contained an unsafe path"
		}
	}
}

New-Item -ItemType Directory -Force -Path $destination | Out-Null
$libDestination = Join-Path $destination "libdatachannel-$libVersion"
$mbedDestination = Join-Path $destination "mbedtls-$mbedVersion"
if ((Test-Path -LiteralPath $libDestination) -or
	(Test-Path -LiteralPath $mbedDestination)) {
	throw "RTC destination already exists; authenticate or remove it explicitly"
}
$staging = Join-Path $destination ".rtc-bootstrap-$([Guid]::NewGuid().ToString('N'))"
$stagingPrefix = $destination.TrimEnd([IO.Path]::DirectorySeparatorChar) +
	[IO.Path]::DirectorySeparatorChar
if (-not ([IO.Path]::GetFullPath($staging)).StartsWith(
	$stagingPrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "RTC staging path escaped the destination"
}
try {
	New-Item -ItemType Directory -Path $staging | Out-Null
	$libStage = Join-Path $staging "libdatachannel-$libVersion"
	Copy-Item -LiteralPath $libSource -Destination $libStage -Recurse
	$gitMetadata = @(Get-ChildItem -LiteralPath $libStage -Force -Recurse |
		Where-Object Name -EQ ".git" |
		Sort-Object FullName -Descending)
	foreach ($metadata in $gitMetadata) {
		Remove-Item -LiteralPath $metadata.FullName -Recurse -Force
	}
	$manifest = @(
		"libdatachannel $libCommit",
		"json $($submodules['deps/json'])",
		"libjuice $($submodules['deps/libjuice'])",
		"libsrtp $($submodules['deps/libsrtp'])",
		"plog $($submodules['deps/plog'])",
		"usrsctp $($submodules['deps/usrsctp'])"
	) -join "`n"
	[IO.File]::WriteAllText(
		(Join-Path $libStage ".th03-source-manifest"),
		$manifest + "`n",
		[Text.UTF8Encoding]::new($false)
	)

	& tar.exe -xf $archive -C $staging
	if ($LASTEXITCODE -ne 0) {
		throw "Mbed TLS archive extraction failed"
	}
	$mbedStageOriginal = Join-Path $staging "mbedtls-$mbedVersion"
	$mbedStage = Join-Path $staging "mbedtls-$mbedVersion"
	[IO.File]::WriteAllText(
		(Join-Path $mbedStage ".th03-source-sha256"),
		$mbedSha256 + "`n",
		[Text.UTF8Encoding]::new($false)
	)

	Move-Item -LiteralPath $libStage -Destination $libDestination
	Move-Item -LiteralPath $mbedStage -Destination $mbedDestination
} finally {
	if (Test-Path -LiteralPath $staging) {
		Remove-Item -LiteralPath $staging -Recurse -Force
	}
}

Write-Output $libDestination
Write-Output $mbedDestination
