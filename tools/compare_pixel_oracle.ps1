[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Base,
	[Parameter(Mandatory = $true)]
	[string]$Candidate,
	[string]$ReportPath,
	[switch]$AllowCandidateSuffix,
	[switch]$AllowBaseSuffix
)

$ErrorActionPreference = "Stop"
$header = "stream`tschema`tpublication`tlogical_tick`tcoalesced_ticks`toverlay`treduced_flash`tsource_fnv64`traster_fnv64`ttram_fnv64`tshown_page`traster_mode`tscroll_y`ttone"
$fieldNames = @(
	"stream", "schema", "publication", "logical_tick", "coalesced_ticks",
	"overlay", "reduced_flash", "source_fnv64", "raster_fnv64",
	"tram_fnv64", "shown_page", "raster_mode", "scroll_y", "tone"
)
$hashFields = @(7, 8, 9)
$utf8NoBom = [Text.UTF8Encoding]::new($false)

function Write-Report([Collections.IDictionary]$Report) {
	$json = $Report | ConvertTo-Json -Depth 5
	if($ReportPath) {
		$reportFull = [IO.Path]::GetFullPath($ReportPath)
		$parent = [IO.Path]::GetDirectoryName($reportFull)
		if($parent) {
			[IO.Directory]::CreateDirectory($parent) | Out-Null
		}
		[IO.File]::WriteAllText($reportFull, $json + "`r`n", $utf8NoBom)
	}
	$json
}

function Read-Oracle([string]$Path) {
	$full = [IO.Path]::GetFullPath($Path)
	if(-not [IO.File]::Exists($full)) {
		throw "Pixel-oracle log does not exist: $full"
	}
	$lines = [IO.File]::ReadAllLines($full)
	if($lines.Count -eq 0 -or $lines[0] -cne $header) {
		throw "Pixel-oracle log has an unsupported T3PXH2 header: $full"
	}
	$rows = [Collections.Generic.List[object]]::new()
	for($index = 1; $index -lt $lines.Count; ++$index) {
		if([string]::IsNullOrWhiteSpace($lines[$index])) {
			throw "Pixel-oracle log contains a blank row at line $($index + 1): $full"
		}
		[string[]]$fields = $lines[$index].Split("`t")
		if($fields.Count -ne $fieldNames.Count) {
			throw "Pixel-oracle row $($index + 1) has $($fields.Count) fields, expected $($fieldNames.Count): $full"
		}
		if($fields[0] -cne "T3PXH" -or $fields[1] -cne "2") {
			throw "Pixel-oracle row $($index + 1) has an unsupported stream/schema: $full"
		}
		if($fields[2] -cne ([string]($index - 1))) {
			throw "Pixel-oracle publication ordinal is not contiguous at line $($index + 1): $full"
		}
		foreach($field in $hashFields) {
			if($fields[$field] -cne "na") {
				$fields[$field] = $fields[$field].ToLowerInvariant()
			}
		}
		$rows.Add($fields)
	}
	if($rows.Count -eq 0) {
		throw "Pixel-oracle log contains no publication rows: $full"
	}
	return $rows.ToArray()
}

try {
	$baseRows = @(Read-Oracle $Base)
	$candidateRows = @(Read-Oracle $Candidate)
} catch {
	$report = [ordered]@{
		status = "malformed"
		base_rows = 0
		candidate_rows = 0
		compared_rows = 0
		first_mismatch = $null
		error = $_.Exception.Message
	}
	Write-Report $report
	exit 1
}

$compared = [Math]::Min($baseRows.Count, $candidateRows.Count)
$firstMismatch = $null
for($row = 0; $row -lt $compared -and $null -eq $firstMismatch; ++$row) {
	for($field = 0; $field -lt $fieldNames.Count; ++$field) {
		$baseValue = $baseRows[$row][$field]
		$candidateValue = $candidateRows[$row][$field]
		if($baseValue -cne $candidateValue) {
			$firstMismatch = [ordered]@{
				publication = $row
				logical_tick = $baseRows[$row][3]
				field = $fieldNames[$field]
				base = $baseValue
				candidate = $candidateValue
			}
			break
		}
	}
}

$lengthAccepted = $baseRows.Count -eq $candidateRows.Count -or
	($AllowCandidateSuffix -and $candidateRows.Count -gt $baseRows.Count) -or
	($AllowBaseSuffix -and $baseRows.Count -gt $candidateRows.Count)
$status = if($null -ne $firstMismatch) {
	"mismatch"
} elseif(-not $lengthAccepted) {
	"length-mismatch"
} elseif($baseRows.Count -eq $candidateRows.Count) {
	"same"
} else {
	"same-prefix"
}
$report = [ordered]@{
	status = $status
	base_rows = $baseRows.Count
	candidate_rows = $candidateRows.Count
	compared_rows = $compared
	first_mismatch = $firstMismatch
	error = $null
}
Write-Report $report
if($status -notin @("same", "same-prefix")) {
	exit 1
}
