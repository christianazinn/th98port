#Requires -Version 7.0
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Artifact,
	[Parameter(Mandatory = $true)]
	[string]$Output,
	[Parameter(Mandatory = $true)]
	[string]$PrivateKey,
	[Parameter(Mandatory = $true)]
	[ValidateSet(
		"windows-standalone", "windows-x64", "linux-x64",
		"macos-x64", "macos-arm64"
	)]
	[string]$Platform,
	[Parameter(Mandatory = $true)]
	[ValidateSet("stable", "beta")]
	[string]$Channel,
	[Parameter(Mandatory = $true)]
	[ValidateRange(1, [long]::MaxValue)]
	[long]$Sequence,
	[Parameter(Mandatory = $true)]
	[ValidateLength(1, 32)]
	[string]$Version,
	[Parameter(Mandatory = $true)]
	[string]$ArtifactUrl,
	[string]$ReleaseNotesUrl = "",
	[long]$PublishedUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds(),
	[string]$PublicKeyOutput = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if(-not [BitConverter]::IsLittleEndian) {
	throw "Update manifest authoring requires a little-endian host"
}

function Assert-Ascii([string]$Value, [string]$Label, [int]$Maximum) {
	if($Value.Length -eq 0 -or $Value.Length -gt $Maximum) {
		throw "$Label length is outside 1..$Maximum bytes"
	}
	foreach($character in $Value.ToCharArray()) {
		$codePoint = [int]$character
		if($codePoint -lt 0x21 -or $codePoint -gt 0x7E) {
			throw "$Label must contain printable ASCII without spaces"
		}
	}
}

function Assert-Https([string]$Value, [string]$Label, [bool]$AllowEmpty) {
	if($Value.Length -eq 0 -and $AllowEmpty) { return }
	Assert-Ascii $Value $Label 768
	$uri = $null
	if(-not [Uri]::TryCreate($Value, [UriKind]::Absolute, [ref]$uri) -or
		$uri.Scheme -cne "https" -or -not [string]::IsNullOrEmpty($uri.UserInfo) -or
		-not [string]::IsNullOrEmpty($uri.Fragment) -or $Value.Contains("\")) {
		throw "$Label must be an absolute credential-free HTTPS URL without a fragment"
	}
}

$artifactPath = [IO.Path]::GetFullPath($Artifact)
$keyPath = [IO.Path]::GetFullPath($PrivateKey)
$outputPath = [IO.Path]::GetFullPath($Output)
if(-not [IO.File]::Exists($artifactPath)) { throw "Artifact not found: $artifactPath" }
if(-not [IO.File]::Exists($keyPath)) { throw "Private key not found: $keyPath" }
$artifactInfo = [IO.FileInfo]::new($artifactPath)
if($artifactInfo.Length -le 0 -or $artifactInfo.Length -gt 1GB) {
	throw "Artifact size is outside 1..1 GiB"
}
if($PublishedUnix -le 0) { throw "PublishedUnix must be positive" }
Assert-Ascii $Version "Version" 32
Assert-Https $ArtifactUrl "ArtifactUrl" $false
Assert-Https $ReleaseNotesUrl "ReleaseNotesUrl" $true

$platformValues = @{
	"windows-standalone" = 1; "windows-x64" = 2; "linux-x64" = 3
	"macos-x64" = 4; "macos-arm64" = 5
}
$channelValues = @{ "stable" = 1; "beta" = 2 }
$versionBytes = [Text.Encoding]::ASCII.GetBytes($Version)
$artifactUrlBytes = [Text.Encoding]::ASCII.GetBytes($ArtifactUrl)
$notesUrlBytes = [Text.Encoding]::ASCII.GetBytes($ReleaseNotesUrl)
$headerSize = 128
$signatureSize = 64
$signedSize = $headerSize + $versionBytes.Length + $artifactUrlBytes.Length +
	$notesUrlBytes.Length
$totalSize = $signedSize + $signatureSize
if($totalSize -gt 2048) { throw "Manifest exceeds 2048 bytes" }

$key = [Security.Cryptography.ECDsa]::Create()
try {
	$key.ImportFromPem([IO.File]::ReadAllText($keyPath))
	if($key.KeySize -ne 256) { throw "Private key must use ECDSA P-256" }
	$publicKey = $key.ExportSubjectPublicKeyInfo()
	[byte[]]$keyId = ([Security.Cryptography.SHA256]::HashData($publicKey))[0..15]
	$artifactDigest = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash
	$artifactDigestBytes = [Convert]::FromHexString($artifactDigest)
	$bytes = [byte[]]::new($totalSize)
	[Text.Encoding]::ASCII.GetBytes("T3UPD001").CopyTo($bytes, 0)
	[BitConverter]::GetBytes([uint16]1).CopyTo($bytes, 8)
	[BitConverter]::GetBytes([uint16]$headerSize).CopyTo($bytes, 10)
	[BitConverter]::GetBytes([uint32]$totalSize).CopyTo($bytes, 12)
	[BitConverter]::GetBytes([uint32]$signedSize).CopyTo($bytes, 16)
	$bytes[20] = [byte]$platformValues[$Platform]
	$bytes[21] = [byte]$channelValues[$Channel]
	[BitConverter]::GetBytes([uint64]$Sequence).CopyTo($bytes, 24)
	[BitConverter]::GetBytes([uint64]$PublishedUnix).CopyTo($bytes, 32)
	[BitConverter]::GetBytes([uint64]$artifactInfo.Length).CopyTo($bytes, 40)
	$artifactDigestBytes.CopyTo($bytes, 48)
	$keyId.CopyTo($bytes, 80)
	[BitConverter]::GetBytes([uint16]$versionBytes.Length).CopyTo($bytes, 96)
	[BitConverter]::GetBytes([uint16]$artifactUrlBytes.Length).CopyTo($bytes, 98)
	[BitConverter]::GetBytes([uint16]$notesUrlBytes.Length).CopyTo($bytes, 100)
	[BitConverter]::GetBytes([uint16]$signatureSize).CopyTo($bytes, 102)
	$cursor = $headerSize
	foreach($text in @($versionBytes, $artifactUrlBytes, $notesUrlBytes)) {
		$text.CopyTo($bytes, $cursor)
		$cursor += $text.Length
	}
	$signedPrefix = [byte[]]::new($signedSize)
	[Array]::Copy($bytes, 0, $signedPrefix, 0, $signedSize)
	$signature = $key.SignData(
		$signedPrefix,
		[Security.Cryptography.HashAlgorithmName]::SHA256,
		[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation
	)
	if($signature.Length -ne $signatureSize -or -not $key.VerifyData(
		$signedPrefix, $signature,
		[Security.Cryptography.HashAlgorithmName]::SHA256,
		[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation
	)) { throw "Generated ECDSA signature did not verify" }
	$signature.CopyTo($bytes, $signedSize)
	[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) |
		Out-Null
	$temporary = $outputPath + ".tmp-" + [Guid]::NewGuid().ToString("N")
	try {
		[IO.File]::WriteAllBytes($temporary, $bytes)
		[IO.File]::Move($temporary, $outputPath, $true)
	} finally {
		if([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
	}
	if(-not [string]::IsNullOrWhiteSpace($PublicKeyOutput)) {
		$publicPath = [IO.Path]::GetFullPath($PublicKeyOutput)
		[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($publicPath)) |
			Out-Null
		[IO.File]::WriteAllText(
			$publicPath,
			"key-id=" + [Convert]::ToHexString($keyId).ToLowerInvariant() + "`n" +
			"spki-base64=" + [Convert]::ToBase64String($publicKey) + "`n",
			[Text.UTF8Encoding]::new($false)
		)
	}
	Write-Output "manifest=$outputPath"
	Write-Output "key-id=$([Convert]::ToHexString($keyId).ToLowerInvariant())"
	Write-Output "artifact-sha256=$($artifactDigest.ToLowerInvariant())"
} finally {
	$key.Dispose()
}
