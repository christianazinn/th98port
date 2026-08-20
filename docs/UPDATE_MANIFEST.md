# PoDD Port update manifest

`T3UPD001` is the platform-neutral signed release-metadata boundary for the
future PoDD Port updater. It does not enable network access or replacement by
itself. Those owners must verify a pinned signing key, download to staging,
validate the artifact, and apply or roll back outside source-time gameplay.

All integers are little-endian. The signature is ECDSA P-256 over SHA-256 in
fixed 64-byte IEEE P1363 `r || s` form. It covers every byte before the
signature, including all length fields and text.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `T3UPD001` |
| 8 | 2 | format version, `1` |
| 10 | 2 | header size, `128` |
| 12 | 4 | complete file size |
| 16 | 4 | signed-prefix size |
| 20 | 1 | platform |
| 21 | 1 | channel |
| 22 | 2 | flags, zero |
| 24 | 8 | monotonic release sequence |
| 32 | 8 | publication Unix time, display/audit only |
| 40 | 8 | artifact byte length |
| 48 | 32 | artifact SHA-256 |
| 80 | 16 | first 16 bytes of SHA-256(SPKI public key) |
| 96 | 2 | display-version byte length |
| 98 | 2 | artifact HTTPS URL byte length |
| 100 | 2 | optional release-notes HTTPS URL byte length |
| 102 | 2 | signature size, `64` |
| 104 | 24 | zero reserved bytes |
| 128 | variable | ASCII version, artifact URL, notes URL |
| signed-prefix size | 64 | signature |

Platforms are Windows standalone `1`, Windows x86-64 package `2`, Linux
x86-64 `3`, macOS Intel `4`, and macOS Apple Silicon `5`. Channels are Stable
`1` and Beta `2`.

The sequence is the sole update-order authority. Version text and timestamps
are never compared, preventing lexical-version mistakes, clock rollback, and a
signed old artifact from replacing a newer installation. A downloader must
also require the requested platform/channel, exact artifact length/SHA-256,
and a locally pinned key ID before making a staged artifact executable.

`authenticate_update_manifest()` is the runtime authentication boundary in
SDL/RTC product builds. It reparses the complete document, requires exactly one
caller-pinned key ID, recomputes the first 16 bytes of SHA-256 over that pin's
complete DER SubjectPublicKeyInfo, rejects every non-ECDSA or non-P-256 key,
and verifies the P1363 signature over the exact parsed prefix through the
authenticated Mbed TLS 3.6.7 provider. The dependency-free parser remains
available to headless/core builds, but structural parsing alone is never an
authorization decision.

`stage_authenticated_update()` extends that boundary without making the
artifact executable. It authenticates the manifest again, requires the exact
requested platform/channel and a strictly higher release sequence, hashes the
complete artifact, and compares both byte length and SHA-256 before touching
the filesystem. It generates a local `.stage` filename from authenticated
numeric fields and a digest prefix rather than either URL, creates an exclusive
same-directory temporary with non-executable owner-only permissions on POSIX,
flushes it, re-reads and re-hashes it, and publishes without replacing an
existing different target. Concurrent or repeated identical staging is
idempotent after complete target revalidation. This still performs no network
I/O, product replacement, or execution.

`tools/release/write_update_manifest.ps1` authors the format from a
caller-owned PEM private key. Private keys and original content are never
repository inputs. Key generation, offline custody, production public-key pin
selection/rotation, download, atomic replacement, recovery, and visible opt-in
policy remain separate release-infrastructure gates.
