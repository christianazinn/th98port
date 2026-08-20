# PoDD Port update package

`T3PKG001` is the portable uncompressed product container named by a signed
`T3UPD001` manifest. It exists to make extraction and generation publication a
small auditable transaction; ZIP and tar archives remain human-distribution
formats and are never updater inputs.

All integers are little-endian. The complete package is limited to 256 MiB.

## Header

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | ASCII `T3PKG001` |
| 8 | 2 | version, exactly 1 |
| 10 | 2 | header size, exactly 32 |
| 12 | 4 | file count, 1..512 |
| 16 | 8 | table bytes following the header |
| 24 | 8 | concatenated payload bytes |

Exactly `file_count` variable entries fill the table. Each entry is:

| Relative offset | Size | Field |
|---:|---:|---|
| 0 | 2 | path bytes, 1..240 |
| 2 | 2 | flags; bit 0 means owner-executable, all others zero |
| 4 | 4 | reserved, zero |
| 8 | 8 | member byte length |
| 16 | 32 | SHA-256 of the exact member bytes |
| 48 | variable | portable ASCII relative path, without a terminator |

Payloads follow the complete table contiguously in entry order. Their declared
sizes must consume the payload region exactly; no alignment, compression,
trailer, or trailing bytes exist.

## Path and tree contract

Paths use `/`, are strictly ordinal-sorted, and are unique under ASCII
case-folding. Empty, absolute, dot, dot-dot, repeated-separator, backslash,
control/non-ASCII, trailing-dot/space, Windows-forbidden punctuation, and
Windows device-name components are rejected. The format represents regular
files only. Directories are implied by member paths, so links, alternate data
streams, devices, empty directories, and platform-specific path aliases cannot
enter a generation.

The author marks `.exe` files executable on Windows-built packages and reads
the owner-execute bit on Linux/macOS. The materializer ignores this bit on
Windows and publishes exact owner-only file/directory modes on POSIX.

## Authorization and materialization

`parse_update_package()` is structural only. It does not authorize bytes.
`materialize_authenticated_update_package()` repeats the signed-manifest key,
platform, channel, sequence, complete size, and complete SHA-256 checks before
parsing. It then hashes every in-memory member before creating the generation
root.

Members are written to an exclusive same-root temporary directory, flushed,
re-read, and re-hashed. The complete directory is published with a platform
no-replace rename as `PODD-GEN-<sequence>-<digest-prefix>`. An existing exact
tree is idempotent; a missing, changed, linked, permission-mismatched, or extra
member is a conflict and is never replaced. The caller's user-data tree is not
part of the package.

This boundary is deliberately inert. It does not select an active generation,
launch a process, acknowledge health, roll back, clean generations, or replace
the executable currently running. Those policies require a separate startup
transaction. `UPDATE_ACTIVATION.md` defines the platform-neutral durable state
for that transaction without yet adding a bootstrap launcher.

`tools/release/write_update_package.ps1` deterministically authors the format
from one installed package root. Manual beta jobs retain their ordinary ZIP or
tarball and additionally upload the corresponding `.t3pkg` for updater testing.
