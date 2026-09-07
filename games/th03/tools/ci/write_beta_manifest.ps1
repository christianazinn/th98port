param(
	[Parameter(Mandatory = $true)]
	[string]$PackageRoot,
	[Parameter(Mandatory = $true)]
	[string]$Platform,
	[Parameter(Mandatory = $true)]
	[string]$Revision,
	[string]$Label = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$root = [IO.Path]::GetFullPath($PackageRoot)
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
	throw "Beta package root does not exist: $root"
}
foreach ($value in @($Platform, $Revision, $Label)) {
	if ($value.Contains("`r") -or $value.Contains("`n")) {
		throw "Beta manifest values must be single-line"
	}
}

$manifestPath = Join-Path $root "BUILD-MANIFEST.txt"
$records = [Collections.Generic.List[string]]::new()
$records.Add("format=TH03-BETA-BUILD-1")
$records.Add("platform=$Platform")
$records.Add("revision=$Revision")
$records.Add("label=$Label")
$records.Add("")
$records.Add("sha256  bytes  relative-path")
foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse |
	Where-Object FullName -NE $manifestPath |
	Sort-Object FullName) {
	$relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace("\", "/")
	$hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
	$records.Add("$($hash.ToLowerInvariant())  $($file.Length)  $relative")
}
[IO.File]::WriteAllLines(
	$manifestPath,
	$records,
	[Text.UTF8Encoding]::new($false)
)
Write-Output $manifestPath
