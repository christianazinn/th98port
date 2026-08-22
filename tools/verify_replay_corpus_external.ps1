#Requires -Version 5.1
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$EvidenceRoot,
	[string]$Verifier,
	[string]$Enedat,
	[string]$V14CorpusRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if(-not $V14CorpusRoot) {
	$V14CorpusRoot = Join-Path $EvidenceRoot "corpus\v14s"
}
$V14CorpusRoot = [IO.Path]::GetFullPath($V14CorpusRoot)
if(-not $Verifier) {
	$Verifier = Join-Path $sourceRoot "out\build\msvc-x64\Release\th03rv.exe"
}
if(-not $Enedat) {
	$Enedat = Join-Path $EvidenceRoot "originals\th03\dat\ENEDAT.DAT"
}
$Verifier = [IO.Path]::GetFullPath($Verifier)
$Enedat = [IO.Path]::GetFullPath($Enedat)
if(-not (Test-Path -LiteralPath $Verifier -PathType Leaf)) {
	throw "Replay verifier does not exist: $Verifier"
}
if(-not (Test-Path -LiteralPath $Enedat -PathType Leaf)) {
	throw "ENEDAT does not exist: $Enedat"
}

$lockPath = Join-Path $sourceRoot "docs\REPLAY_V14_CORPUS_LOCK.tsv"
$locked = @(Import-Csv -LiteralPath $lockPath -Delimiter "`t")
if($locked.Count -eq 0) {
	throw "Replay corpus lock is empty: $lockPath"
}

$passed = 0
$failed = 0
foreach($entry in $locked) {
	$replay = Join-Path $V14CorpusRoot $entry.file
	if(-not (Test-Path -LiteralPath $replay -PathType Leaf)) {
		throw "Locked converted V14 replay is missing: $replay"
	}
	$item = Get-Item -LiteralPath $replay
	if($item.Length -ne [int64]$entry.bytes) {
		throw "$($entry.file) size disagrees with the converted V14 corpus lock."
	}
	$sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $replay).Hash
	if($sha256.ToLowerInvariant() -cne $entry.sha256) {
		throw "$($entry.file) SHA-256 disagrees with the converted V14 corpus lock."
	}

	$output = @(& $Verifier $replay --enedat $Enedat 2>&1)
	if($LASTEXITCODE -eq 0 -and $output -ccontains "verification=match") {
		$passed++
		Write-Host "PASS V14/$($entry.file)"
	} else {
		$failed++
		Write-Host "FAIL V14/$($entry.file)"
		$output | Where-Object {
			$_ -match '^(split=.*match=no|run_status=|consumed_samples=|verification=)'
		} | ForEach-Object { Write-Host "  $_" }
	}
}

Write-Host "replays_passed=$passed"
Write-Host "replays_failed=$failed"
if($failed -ne 0) {
	throw "$failed replay verification run(s) did not match."
}
