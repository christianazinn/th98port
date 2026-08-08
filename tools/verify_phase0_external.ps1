#Requires -Version 5.1
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$EvidenceRoot,
	[string]$ReplayCheck,
	[string]$RunTag = "final-0807-explicit-prev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if(-not $ReplayCheck) {
	$ReplayCheck = Join-Path $sourceRoot "out\build\msvc-x64\Debug\th03-replay-check.exe"
}
$ReplayCheck = [IO.Path]::GetFullPath($ReplayCheck)
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if(-not (Test-Path -LiteralPath $ReplayCheck -PathType Leaf)) {
	throw "Replay checker does not exist: $ReplayCheck"
}

$cases = @(
	@("TH3R00", "4675bd3d4c2cd55d5402843923f39fe78d5994548e6d6ce5f146496d17057246", "626b77d4f00802e7ee065faf813351e5344e62d677004913f534fe9ea330cb7c", 100642),
	@("TH3R01", "3be1059a75815702b0bca0f9f02eda1697c8308f3ae1fcb20dbe6a0e27f8dac9", "b5347245a27929f590dcaa39e72ce09f3e3c6f6a4ff0d2a456876df9b7f47ba3", 135215),
	@("TH3R02", "e378d1e5807c984d9e7cd139a27a4d1c4a054484cda4b5f835297e1a90b6d2b1", "15a906f8680b9b324456e627fccd6c90939d834e7acf4132021b93348ca1d8c4", 91939),
	@("TH3R03", "d07232f2559f8b9015fdf2942cdffcf0518abdc78e43b1b638e83dd9d277b77e", "62b28fc4910abac56cf5677ad56bf8306292b8a4027775a85703cdb0b0485ef8", 25211),
	@("TH3R04", "468ca6d2a33fc3f14b088a179451296d5b16efa197249286279b0d7999089070", "98e516ecc1a8dac38514ed959fd5410373af01c2952142084aa87fb05e1b0c6f", 86437),
	@("TH3R05", "e674542eb9515358cb17c0afbe7a2c8f701bd959c0d2ea8f5e7e85a75fdcf38a", "4a007c121694a81c6bead0404029dfda791813ee06a0a461fb0cbe676bdfc6d5", 111747)
)

function Invoke-Checked([string[]]$Arguments) {
	$output = @(& $ReplayCheck @Arguments)
	if($LASTEXITCODE -ne 0) {
		throw "$ReplayCheck $($Arguments -join ' ') failed with exit $LASTEXITCODE.`n$($output -join "`n")"
	}
	return $output
}

$validatedDocuments = 0
$comparisons = 0
foreach($case in $cases) {
	$name = [string]$case[0]
	$lower = $name.ToLowerInvariant()
	$v11 = Join-Path $EvidenceRoot "corpus\v11s\$name.RPY"
	$t3case = Join-Path $EvidenceRoot ".scratch\port\phase0\cases\$name.T3C"
	if(-not (Test-Path -LiteralPath $v11 -PathType Leaf)) { throw "Missing V11: $v11" }
	if(-not (Test-Path -LiteralPath $t3case -PathType Leaf)) { throw "Missing T3CASE: $t3case" }
	if((Get-FileHash -Algorithm SHA256 -LiteralPath $v11).Hash.ToLowerInvariant() -cne $case[1]) {
		throw "$name V11 SHA-256 disagrees with the Phase 0 freeze."
	}
	if((Get-FileHash -Algorithm SHA256 -LiteralPath $t3case).Hash.ToLowerInvariant() -cne $case[2]) {
		throw "$name T3CASE SHA-256 disagrees with the Phase 0 freeze."
	}
	$caseOutput = Invoke-Checked @("--case", $t3case)
	$v11Output = Invoke-Checked @("--v11", $v11)
	if($caseOutput -cnotcontains "samples=$($case[3])") {
		throw "$name T3CASE sample count is not $($case[3])."
	}
	if($v11Output -cnotcontains "samples=$($case[3])") {
		throw "$name V11 sample count is not $($case[3])."
	}
	$validatedDocuments += 2

	$master = Join-Path $EvidenceRoot ".scratch\emu\t3case\phase0-$lower-master-$RunTag\game\T3SPLIT.BIN"
	$anniversary = Join-Path $EvidenceRoot ".scratch\emu\t3case\phase0-$lower-anniversary-$RunTag\game\T3SPLIT.BIN"
	$replays = Join-Path $EvidenceRoot ".scratch\emu\replay\phase0-$lower-replays-$RunTag\game\T3SPLIT.BIN"
	Invoke-Checked @("--compare-splits", $master, $anniversary) | Out-Null
	Invoke-Checked @("--compare-splits", $master, $replays) | Out-Null
	$comparisons += 2
	Write-Host "PASS $name"
}

Write-Host "external_documents=$validatedDocuments"
Write-Host "split_comparisons=$comparisons"
