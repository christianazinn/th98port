#Requires -Version 7.0
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Signer,
	[Parameter(Mandatory = $true)]
	[string]$Artifact
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$temporary = [IO.Directory]::CreateTempSubdirectory("th03-update-manifest-")
try {
	$privateKey = Join-Path $temporary.FullName "ephemeral-private.pem"
	$manifest = Join-Path $temporary.FullName "test.t3upd"
	$public = Join-Path $temporary.FullName "public.txt"
	$key = [Security.Cryptography.ECDsa]::Create(
		[Security.Cryptography.ECCurve]::CreateFromFriendlyName("nistP256")
	)
	try {
		[IO.File]::WriteAllText(
			$privateKey,
			$key.ExportPkcs8PrivateKeyPem(),
			[Text.UTF8Encoding]::new($false)
		)
	} finally {
		$key.Dispose()
	}

	& $Signer -Artifact $Artifact -Output $manifest -PrivateKey $privateKey `
		-Platform windows-x64 -Channel beta -Sequence 5001 `
		-Version "0.5.0-beta.1" `
		-ArtifactUrl "https://azinn.dev/th03/beta/test.exe" `
		-ReleaseNotesUrl "https://azinn.dev/th03/releases/0.5.0" `
		-PublishedUnix 1787000000 -PublicKeyOutput $public | Out-Null

	$bytes = [IO.File]::ReadAllBytes($manifest)
	if([Text.Encoding]::ASCII.GetString($bytes, 0, 8) -cne "T3UPD001") {
		throw "Generated manifest magic is wrong"
	}
	$totalSize = [BitConverter]::ToUInt32($bytes, 12)
	$signedSize = [BitConverter]::ToUInt32($bytes, 16)
	if($totalSize -ne $bytes.Length -or $bytes.Length - $signedSize -ne 64) {
		throw "Generated manifest lengths are inconsistent"
	}
	$prefix = [byte[]]::new($signedSize)
	$signature = [byte[]]::new(64)
	[Array]::Copy($bytes, 0, $prefix, 0, $signedSize)
	[Array]::Copy($bytes, $signedSize, $signature, 0, 64)
	$spkiLine = Get-Content -LiteralPath $public |
		Where-Object { $_.StartsWith("spki-base64=", [StringComparison]::Ordinal) }
	if($null -eq $spkiLine) { throw "Public-key output lacks SPKI" }
	$spki = [Convert]::FromBase64String($spkiLine.Substring(12))
	$verifier = [Security.Cryptography.ECDsa]::Create()
	try {
		$bytesRead = 0
		$verifier.ImportSubjectPublicKeyInfo($spki, [ref]$bytesRead)
		if($bytesRead -ne $spki.Length -or -not $verifier.VerifyData(
			$prefix,
			$signature,
			[Security.Cryptography.HashAlgorithmName]::SHA256,
			[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation
		)) {
			throw "Independent signature verification failed"
		}
	} finally {
		$verifier.Dispose()
	}

	$artifactDigest = [Convert]::FromHexString(
		(Get-FileHash -LiteralPath $Artifact -Algorithm SHA256).Hash
	)
	for($index = 0; $index -lt $artifactDigest.Length; ++$index) {
		if($bytes[48 + $index] -ne $artifactDigest[$index]) {
			throw "Generated manifest artifact digest is wrong"
		}
	}
	Write-Output "update-manifest-authoring-test=passed bytes=$($bytes.Length)"
} finally {
	$temporary.Delete($true)
}
