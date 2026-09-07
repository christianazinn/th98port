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

`acquire_and_stage_update()` owns the next host-side boundary. It first asks a
bounded transport for at most 2,048 manifest bytes, authenticates those exact
bytes, and rejects the wrong platform, channel, or a non-newer sequence before
it makes any artifact request. Only then does it request the authenticated
artifact URL with the authenticated byte length as the response ceiling. A
200 response of exactly that length enters `stage_authenticated_update()`,
which independently repeats signature, size, and SHA-256 verification. The
runtime acquisition ceiling is 256 MiB even though the format can describe a
larger future package. Network, status, selection, length, digest, and staging
failures leave the product and user-data trees unchanged.

`acquire_update_payload()` exposes that same manifest-first bounded transaction
to later host owners without staging a duplicate artifact file. Its returned
bytes remain inert: both staging and generation materialization independently
repeat signature, platform/channel/sequence, exact length, and complete
artifact-digest verification before filesystem mutation.

`install_update_for_next_launch()` joins the bounded acquisition result to
`T3PKG001` materialization and the alternating activation store. It loads the
activation sequence before making a request and takes the maximum of that
value and the caller's compiled installed-release sequence. This prevents a
fresh legacy install with no `PODD.UPDATE` state from accepting a correctly
signed downgrade. It publishes only the exact marked immutable generation,
then nominates that same full sequence/SHA-256 identity as `pending`.
Activation commits retry only a stale revision; an unrelated busy transition
or newer release wins, leaving at most an inert generation for the conservative
cleanup pass. It never replaces or relaunches the running application.

The release trust boundary is default-off. A configured build must explicitly
supply all five CMake inputs:

- `TH03_UPDATE_MANIFEST_URL`, a credential-free absolute HTTPS URL;
- `TH03_UPDATE_PLATFORM`, one exact `T3UPD001` platform;
- `TH03_UPDATE_CHANNEL`, `stable` or `beta`;
- `TH03_UPDATE_CURRENT_SEQUENCE`, the nonzero monotonic sequence compiled into
  that release; and
- `TH03_UPDATE_PUBLIC_KEY_FILE`, a DER P-256 SubjectPublicKeyInfo whose bytes
  and SHA-256-derived key ID are compiled into the executable.

Supplying only some inputs is a configure error. Supplying none produces no
compiled release configuration and therefore no update capability. Runtime
files, settings, command-line arguments, and environment variables cannot
replace the compiled trust root.

The authenticated artifact is a `T3PKG001` product tree rather than a ZIP or
tar archive. Its strict member/path/digest format and inert generation
materialization boundary are specified in `UPDATE_PACKAGE.md`.

`windows_update_transport()` supplies the Windows platform adapter. WinHTTP
accepts absolute ASCII HTTPS URLs only, uses the system proxy and certificate
policy, forbids credentials, fragments, backslashes, and redirects, applies
connect/request timeouts, and enforces the body limit while reading. The
adapter is synchronous by design and must be called by a host worker; no
source-time owner calls it.

`curl_update_transport()` supplies the equivalent Linux and macOS boundary.
It accepts the same restricted absolute-HTTPS vocabulary, pins libcurl to the
HTTPS protocol even if a future caller enables redirects, keeps redirects
disabled, requires ordinary certificate and host verification, applies the
same connect/request timeouts, and aborts while receiving if the signed bound
would be exceeded. Linux beta builds install the distribution's OpenSSL-backed
libcurl development package; macOS packages use the SDK-provided libcurl. The
adapter owns acquisition only. It does not authorize, apply, or execute an
update.

`tools/release/write_update_manifest.ps1` authors the format from a
caller-owned PEM private key. Private keys and original content are never
repository inputs. Key generation, offline custody, production public-key pin
selection/rotation, origin provisioning, and visible opt-in policy remain
separate release-infrastructure gates.
