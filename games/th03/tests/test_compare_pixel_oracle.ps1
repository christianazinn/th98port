[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Comparator
)

$ErrorActionPreference = "Stop"
$root = Join-Path ([IO.Path]::GetTempPath()) (
	"th03-pixel-oracle-test-" + [Guid]::NewGuid().ToString("N")
)
[IO.Directory]::CreateDirectory($root) | Out-Null
$utf8NoBom = [Text.UTF8Encoding]::new($false)
$header = "stream`tschema`tpublication`tlogical_tick`tcoalesced_ticks`toverlay`treduced_flash`tsource_fnv64`traster_fnv64`ttram_fnv64`tshown_page`traster_mode`tscroll_y`ttone"
$row0 = "T3PXH`t2`t0`t17`t1`t0`t0`tABCDEF0123456789`t1122334455667788`t8877665544332211`t1`t0`t0`t100"
$row1 = "T3PXH`t2`t1`t18`t1`t1`t0`t0123456789ABCDEF`tna`tna`t0`t0`t0`t100"
$checks = 0

function Assert([bool]$Condition, [string]$Message) {
	$script:checks++
	if(-not $Condition) { throw $Message }
}

function Write-Capture([string]$Name, [string[]]$Rows) {
	$path = Join-Path $root $Name
	[IO.File]::WriteAllText(
		$path, (($Rows -join "`r`n") + "`r`n"), $utf8NoBom
	)
	return $path
}

function Invoke-OracleComparison(
	[string]$Name,
	[string]$Base,
	[string]$Candidate,
	[string[]]$Extra = @()
) {
	$report = Join-Path $root ($Name + ".json")
	$arguments = @(
		"-NoProfile", "-File", $Comparator, "-Base", $Base,
		"-Candidate", $Candidate, "-ReportPath", $report
	) + $Extra
	& pwsh @arguments *> $null
	$code = $LASTEXITCODE
	$data = if([IO.File]::Exists($report)) {
		Get-Content -Raw $report | ConvertFrom-Json
	} else { $null }
	return @{ Code = $code; Data = $data; Report = $report }
}

try {
	$base = Write-Capture "base.tsv" @($header, $row0, $row1)
	$caseOnly = Write-Capture "case.tsv" @(
		$header, $row0.ToLowerInvariant().Replace("t3pxh", "T3PXH"), $row1
	)
	$same = Invoke-OracleComparison "same" $base $caseOnly
	Assert ($same.Code -eq 0 -and $same.Data.status -eq "same") `
		"hex fingerprints must compare case-insensitively"

	$asymmetric = Write-Capture "asymmetric.tsv" @(
		$header, $row0.Replace("1122334455667788", "na"), $row1
	)
	$mismatch = Invoke-OracleComparison "asymmetric" $base $asymmetric
	Assert (
		$mismatch.Code -eq 1 -and $mismatch.Data.status -eq "mismatch" -and
		$mismatch.Data.first_mismatch.field -eq "raster_fnv64"
	) "asymmetric na must be a fingerprint-availability mismatch"

	$longer = Write-Capture "longer.tsv" @(
		$header, $row0, $row1,
		"T3PXH`t2`t2`t19`t1`t0`t0`t0000000000000001`tna`tna`t0`t0`t0`t100"
	)
	$length = Invoke-OracleComparison "length" $base $longer
	Assert ($length.Code -eq 1 -and $length.Data.status -eq "length-mismatch") `
		"unexplained suffixes must fail"
	$prefix = Invoke-OracleComparison "prefix" $base $longer @("-AllowCandidateSuffix")
	Assert ($prefix.Code -eq 0 -and $prefix.Data.status -eq "same-prefix") `
		"explicit candidate suffix allowance must pass only the common prefix"

	$malformed = Write-Capture "malformed.tsv" @($header, $row0, "")
	$bad = Invoke-OracleComparison "malformed" $base $malformed
	Assert (
		$bad.Code -eq 1 -and $bad.Data.status -eq "malformed" -and
		$bad.Data.error
	) "malformed input must still produce a structured report"
	$bytes = [IO.File]::ReadAllBytes($bad.Report)
	Assert (
		$bytes.Length -ge 3 -and -not (
			$bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
		)
	) "JSON reports must be UTF-8 without a BOM"

	Write-Output "checks=$checks failures=0"
} finally {
	Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
