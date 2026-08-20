#Requires -Version 7.0
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Signer,
	[Parameter(Mandatory = $true)]
	[string]$Artifact,
	[string]$Verifier = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-ReboundManifest(
	[byte[]]$Source,
	[byte[]]$PublicKey,
	[string]$Path
) {
	[byte[]]$rebound = $Source.Clone()
	$keyDigest = [Security.Cryptography.SHA256]::HashData($PublicKey)
	[Array]::Copy($keyDigest, 0, $rebound, 80, 16)
	[IO.File]::WriteAllBytes($Path, $rebound)
}

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
	$publicDer = Join-Path $temporary.FullName "public.der"
	[IO.File]::WriteAllBytes($publicDer, $spki)
	$signatureVerifier = [Security.Cryptography.ECDsa]::Create()
	try {
		$bytesRead = 0
		$signatureVerifier.ImportSubjectPublicKeyInfo($spki, [ref]$bytesRead)
		if($bytesRead -ne $spki.Length -or -not $signatureVerifier.VerifyData(
			$prefix,
			$signature,
			[Security.Cryptography.HashAlgorithmName]::SHA256,
			[Security.Cryptography.DSASignatureFormat]::IeeeP1363FixedFieldConcatenation
		)) {
			throw "Independent signature verification failed"
		}
	} finally {
		$signatureVerifier.Dispose()
	}

	$artifactDigest = [Convert]::FromHexString(
		(Get-FileHash -LiteralPath $Artifact -Algorithm SHA256).Hash
	)
	for($index = 0; $index -lt $artifactDigest.Length; ++$index) {
		if($bytes[48 + $index] -ne $artifactDigest[$index]) {
			throw "Generated manifest artifact digest is wrong"
		}
	}
	if(-not [string]::IsNullOrWhiteSpace($Verifier)) {
		$wrongCurveKey = [Security.Cryptography.ECDsa]::Create(
			[Security.Cryptography.ECCurve]::CreateFromFriendlyName("nistP384")
		)
		try {
			$wrongCurveSpki = $wrongCurveKey.ExportSubjectPublicKeyInfo()
		} finally {
			$wrongCurveKey.Dispose()
		}
		$wrongCurvePublic = Join-Path $temporary.FullName "wrong-curve.der"
		$wrongCurveManifest = Join-Path $temporary.FullName "wrong-curve.t3upd"
		[IO.File]::WriteAllBytes($wrongCurvePublic, $wrongCurveSpki)
		Write-ReboundManifest $bytes $wrongCurveSpki $wrongCurveManifest

		$wrongTypeKey = [Security.Cryptography.RSA]::Create(2048)
		try {
			$wrongTypeSpki = $wrongTypeKey.ExportSubjectPublicKeyInfo()
		} finally {
			$wrongTypeKey.Dispose()
		}
		$wrongTypePublic = Join-Path $temporary.FullName "wrong-type.der"
		$wrongTypeManifest = Join-Path $temporary.FullName "wrong-type.t3upd"
		[IO.File]::WriteAllBytes($wrongTypePublic, $wrongTypeSpki)
		Write-ReboundManifest $bytes $wrongTypeSpki $wrongTypeManifest

		[byte[]]$invalidSpki = 1, 2, 3, 4, 5, 6, 7, 8
		$invalidPublic = Join-Path $temporary.FullName "invalid.der"
		$invalidManifest = Join-Path $temporary.FullName "invalid.t3upd"
		[IO.File]::WriteAllBytes($invalidPublic, $invalidSpki)
		Write-ReboundManifest $bytes $invalidSpki $invalidManifest

		& $Verifier $manifest $publicDer `
			$wrongCurveManifest $wrongCurvePublic `
			$wrongTypeManifest $wrongTypePublic `
			$invalidManifest $invalidPublic
		if($LASTEXITCODE -ne 0) {
			throw "Native update authentication test failed with $LASTEXITCODE"
		}
	}
	Write-Output "update-manifest-authoring-test=passed bytes=$($bytes.Length)"
} finally {
	$temporary.Delete($true)
}
