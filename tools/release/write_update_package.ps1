#Requires -Version 7.0
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Root,
	[Parameter(Mandatory = $true)]
	[string]$Output
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$HeaderBytes = 32
$EntryBytes = 48
$MaximumBytes = 256MB
$MaximumFiles = 512
$MaximumPathBytes = 240

function Test-ReservedComponent([string]$Component) {
	$stem = $Component.Split('.')[0].ToLowerInvariant()
	if($stem -in @("con", "prn", "aux", "nul")) { return $true }
	return $stem -match '^(com|lpt)[1-9]$'
}

function Assert-PortablePath([string]$Path) {
	if([string]::IsNullOrEmpty($Path) -or $Path.StartsWith('/') -or
		$Path.EndsWith('/') -or $Path.Length -gt $MaximumPathBytes) {
		throw "Package member path is invalid: $Path"
	}
	foreach($component in $Path.Split('/')) {
		if([string]::IsNullOrEmpty($component) -or $component -in @('.', '..') -or
			$component.EndsWith('.') -or $component.EndsWith(' ') -or
			(Test-ReservedComponent $component)) {
			throw "Package member component is not portable: $Path"
		}
		foreach($character in $component.ToCharArray()) {
			$value = [int]$character
			if($value -lt 0x21 -or $value -gt 0x7E -or
				'\:*?"<>|'.Contains($character)) {
				throw "Package member path is not portable ASCII: $Path"
			}
		}
	}
}

$rootPath = [IO.Path]::GetFullPath($Root)
$outputPath = [IO.Path]::GetFullPath($Output)
if(-not [IO.Directory]::Exists($rootPath)) {
	throw "Package root does not exist: $rootPath"
}
$rootPrefix = $rootPath.TrimEnd(
	[IO.Path]::DirectorySeparatorChar,
	[IO.Path]::AltDirectorySeparatorChar
) + [IO.Path]::DirectorySeparatorChar
if($outputPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
	throw "Output must not be inside the packaged root"
}

$rawMembers = @(
	Get-ChildItem -LiteralPath $rootPath -File -Recurse | ForEach-Object {
		if(($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
			throw "Package roots must not contain linked files: $($_.FullName)"
		}
		$relative = [IO.Path]::GetRelativePath(
			$rootPath, $_.FullName
		).Replace([IO.Path]::DirectorySeparatorChar, '/')
		Assert-PortablePath $relative
		[byte[]]$bytes = [IO.File]::ReadAllBytes($_.FullName)
		$executable = $_.Extension.Equals(
			'.exe', [StringComparison]::OrdinalIgnoreCase
		)
		if(-not $IsWindows) {
			$mode = [IO.File]::GetUnixFileMode($_.FullName)
			$executable = [bool]($mode -band [IO.UnixFileMode]::UserExecute)
		}
		[PSCustomObject]@{
			Path = $relative
			Bytes = $bytes
			Digest = [Security.Cryptography.SHA256]::HashData($bytes)
			Executable = $executable
		}
	}
)
if($rawMembers.Count -eq 0 -or $rawMembers.Count -gt $MaximumFiles) {
	throw "Package must contain 1..$MaximumFiles regular files"
}
$keys = [Collections.Generic.HashSet[string]]::new(
	[StringComparer]::OrdinalIgnoreCase
)
[string[]]$sortedPaths = $rawMembers.Path
[Array]::Sort($sortedPaths, [StringComparer]::Ordinal)
$byPath = @{}
foreach($member in $rawMembers) {
	if(-not $keys.Add($member.Path)) {
		throw "Package contains a case-insensitive path collision: $($member.Path)"
	}
	$byPath.Add($member.Path, $member)
}
$members = @($sortedPaths | ForEach-Object { $byPath[$_] })
[UInt64]$tableSize = 0
[UInt64]$payloadSize = 0
foreach($member in $members) {
	$tableSize += $EntryBytes + $member.Path.Length
	$payloadSize += $member.Bytes.LongLength
}
[UInt64]$totalSize = $HeaderBytes + $tableSize + $payloadSize
if($totalSize -gt $MaximumBytes) {
	throw "Package exceeds the $MaximumBytes-byte runtime ceiling"
}

$outputDirectory = [IO.Path]::GetDirectoryName($outputPath)
if([string]::IsNullOrEmpty($outputDirectory)) {
	$outputDirectory = [Environment]::CurrentDirectory
}
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$temporary = Join-Path $outputDirectory (
	".$([IO.Path]::GetFileName($outputPath)).tmp-$PID-$([Guid]::NewGuid().ToString('N'))"
)
try {
	$stream = [IO.File]::Open(
		$temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write,
		[IO.FileShare]::None
	)
	try {
		$writer = [IO.BinaryWriter]::new(
			$stream, [Text.UTF8Encoding]::new($false, $true), $true
		)
		try {
			$writer.Write([Text.Encoding]::ASCII.GetBytes('T3PKG001'))
			$writer.Write([UInt16]1)
			$writer.Write([UInt16]$HeaderBytes)
			$writer.Write([UInt32]$members.Count)
			$writer.Write([UInt64]$tableSize)
			$writer.Write([UInt64]$payloadSize)
			foreach($member in $members) {
				$pathBytes = [Text.Encoding]::ASCII.GetBytes($member.Path)
				$writer.Write([UInt16]$pathBytes.Length)
				$writer.Write([UInt16]($(if($member.Executable) { 1 } else { 0 })))
				$writer.Write([UInt32]0)
				$writer.Write([UInt64]$member.Bytes.LongLength)
				$writer.Write($member.Digest)
				$writer.Write($pathBytes)
			}
			foreach($member in $members) { $writer.Write($member.Bytes) }
		} finally {
			$writer.Dispose()
		}
		$stream.Flush($true)
	} finally {
		$stream.Dispose()
	}
	if((Get-Item -LiteralPath $temporary).Length -ne $totalSize) {
		throw "Generated package length failed read-back validation"
	}
	[IO.File]::Move($temporary, $outputPath, $true)
	Write-Output (
		"update-package=$outputPath files=$($members.Count) bytes=$totalSize " +
		"sha256=$((Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash)"
	)
} finally {
	if([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
}
