param(
	[string]$DownloadRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$outRoot = [IO.Path]::GetFullPath((Join-Path $sourceRoot "out"))
if ([string]::IsNullOrWhiteSpace($DownloadRoot)) {
	$DownloadRoot = Join-Path $outRoot "ci-downloads"
}
$downloads = [IO.Path]::GetFullPath($DownloadRoot)
$outPrefix = $outRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) +
	[IO.Path]::DirectorySeparatorChar
if (-not $downloads.StartsWith(
	$outPrefix,
	[StringComparison]::OrdinalIgnoreCase
)) {
	throw "CI dependency downloads must remain below th03-port/out"
}

$sdlVersion = "3.4.10"
$sdlSha256 =
	"12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785"
$sdlArchive = Join-Path $downloads "SDL3-$sdlVersion.tar.gz"
$sdlUrl =
	"https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL3-$sdlVersion.tar.gz"

$mbedVersion = "3.6.7"
$mbedSha256 =
	"a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6"
$mbedArchive = Join-Path $downloads "mbedtls-$mbedVersion.tar.bz2"
$mbedUrl =
	"https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$mbedVersion/mbedtls-$mbedVersion.tar.bz2"

$libVersion = "0.24.3"
$libCommit = "c6696d157b5612df2a741d9a03b192b47ab6cefb"
$libSource = Join-Path $downloads "libdatachannel-$libVersion-source"
$dependencyRoot = Join-Path $outRoot "deps"
$gitCommand = (Get-Command git -ErrorAction Stop).Source
$stagedDependencies = @(
	Join-Path $dependencyRoot "SDL3-$sdlVersion"
	Join-Path $dependencyRoot "libdatachannel-$libVersion"
	Join-Path $dependencyRoot "mbedtls-$mbedVersion"
)
$stagedCount = @($stagedDependencies | Where-Object {
	Test-Path -LiteralPath $_ -PathType Container
}).Count
if ($stagedCount -eq $stagedDependencies.Count) {
	Write-Output (
		"Pinned dependency trees already exist; CMake will authenticate them: " +
		$dependencyRoot
	)
	exit 0
}
if ($stagedCount -ne 0) {
	throw "RTC/SDL dependency staging is incomplete; refusing a mixed tree"
}

function Get-AuthenticatedFile {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Uri,
		[Parameter(Mandatory = $true)]
		[string]$Destination,
		[Parameter(Mandatory = $true)]
		[string]$Sha256
	)

	if (-not (Test-Path -LiteralPath $Destination -PathType Leaf)) {
		Invoke-WebRequest -Uri $Uri -OutFile $Destination
	}
	$actual = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
	if ($actual -ne $Sha256) {
		throw "Downloaded dependency SHA-256 mismatch: $Destination"
	}
}

New-Item -ItemType Directory -Force -Path $downloads | Out-Null
Get-AuthenticatedFile -Uri $sdlUrl -Destination $sdlArchive -Sha256 $sdlSha256
Get-AuthenticatedFile -Uri $mbedUrl -Destination $mbedArchive -Sha256 $mbedSha256

if (-not (Test-Path -LiteralPath $libSource)) {
	& $gitCommand clone --quiet --recursive --branch "v$libVersion" --depth 1 `
		"https://github.com/paullouisageneau/libdatachannel.git" $libSource
	if ($LASTEXITCODE -ne 0) {
		throw "Pinned libdatachannel clone failed"
	}
}
$head = (& $gitCommand -C $libSource rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $libCommit) {
	throw "libdatachannel tag did not resolve to the pinned commit"
}

& (Join-Path $sourceRoot "tools/bootstrap_sdl3.ps1") `
	-DestinationRoot $dependencyRoot -ArchivePath $sdlArchive
if ($LASTEXITCODE -ne 0) {
	throw "SDL dependency bootstrap failed"
}
& (Join-Path $sourceRoot "tools/bootstrap_rtc.ps1") `
	-DestinationRoot $dependencyRoot `
	-LibDataChannelSource $libSource `
	-MbedTlsArchive $mbedArchive
if ($LASTEXITCODE -ne 0) {
	throw "RTC dependency bootstrap failed"
}

Write-Output "Authenticated beta dependencies are ready in $dependencyRoot"
