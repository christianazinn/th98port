# New Classic online identity and offline cache

Status: `T3ONL002` client/service projection implemented locally. No visible
online route or production network binding is implemented.

## Ownership

HiScore will own score-oriented online views. Replay will own replay browsing,
download, and upload views. The two domains share typed replay/content/ruleset
identities and may link to each other, but neither becomes a menu hub until it
has a second real destination.

The boundary is offline-first:

- gameplay, local score persistence, and local replay recording never depend
  on this cache;
- the cache contains no endpoint, provider, credential, token, or authority
  over a submitted score;
- a replay-verified score must name both an exact replay and a verifier;
- a curated historical score has no replay or verifier identity and does not
  require a public evidence link; and
- downloaded replay filenames are content-addressed local names, never remote
  paths.

## Identities

Every identity is a strongly typed 32-byte SHA-256 digest. Types cannot be
mixed at the C++ API boundary.

| Type | Canonical digest input |
|---|---|
| Replay | Exact validated V14 file bytes |
| Content | Versioned manifest of gameplay-affecting content |
| Ruleset | Versioned category/rule manifest |
| Verifier | Reproducible verifier build/authority manifest |
| Entry | Canonical leaderboard-entry payload |
| Snapshot | Canonical cached response payload |

Generation of the four manifest formats and the final service protocol remain
phase-9 work. This client accepts typed digests but does not invent authority
for them.

## Domain invariants

Categories carry mode, rank, character, and Practice start coordinates. Story
and VS categories require zero start coordinates. Results carry an exact
TH03 score (a multiple of ten), terminal stage, continues used, and a
mode-compatible result kind.

`replay_verified` entries require nonzero replay and verifier identities and
forbid evidence/curator text. `curated_legacy` entries require absent replay
and verifier identities; internal evidence/curator text is optional. When cached
replay metadata is present for a verified entry, its content, ruleset,
category, and result must match the leaderboard entry exactly.

Every fetched document is bound to one exact board category. Replay records
also retain exact Autofire, timed-frame, and slow-frame values and enforce the
service's literal five-percent eligibility ratio. All visible strings are
bounded, control-free UTF-8; an empty saved name is valid. The document is
limited to 256 leaderboard entries, 256 replay records, and 256 KiB.

## `T3ONL002` cache format

All integers are little-endian. The header is 80 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `T3ONL002` |
| 8 | 2 | version `2` |
| 10 | 2 | header size `80` |
| 12 | 4 | total file size |
| 16 | 4 | local atomic generation |
| 20 | 2 | leaderboard-entry count |
| 22 | 2 | replay-record count |
| 24 | 8 | fetch time, Unix seconds |
| 32 | 32 | snapshot identity |
| 64 | 5 | mode, rank, character, start stage, start round |
| 69 | 1 | board-category-present flag |
| 70 | 10 | zero |

Leaderboard records follow in cache order, then replay records. Each begins
with a two-byte total record length and uses fixed typed identities plus
two-byte length-prefixed UTF-8 strings. Optional identities are encoded as 32
zero bytes. One little-endian FNV-1a-32 checksum over every preceding byte
terminates the file.

The leaderboard record order is:

```text
u16 record_bytes
EntryId ContentId RulesetId ReplayId-or-zero VerifierId-or-zero
u8 mode rank character start_stage start_round
u8 result_kind end_stage continues provenance reserved_zero
u64 score
u16 dos_date
u64 published_unix_seconds
u16+bytes player
u16+bytes evidence_source
u16+bytes curator_note
```

The replay record order is:

```text
u16 record_bytes
ReplayId ContentId RulesetId
u8 mode rank character start_stage start_round
u8 result_kind end_stage continues availability reserved_zero
u64 score
u16 dos_date
u32 exact_payload_bytes
u8 autofire
u8[3] zero
u32 timed_frames
u32 slow_frames
u16+bytes title
```

An absent cache has no board, snapshot, or fetch identity. A successfully
fetched empty board has all three and zero records, so offline UI can
distinguish `no cache` from `this board is empty`. `PODD.ONL`, `.tmp`, and
`.old` are a fixed atomic recovery family; the greatest valid generation wins.
A present but wholly corrupt family returns an error rather than being silently
reinterpreted as online data.

Downloaded payloads use `<64 uppercase replay-id hex digits>.RPY`. The cache
only records metadata and availability; V14 validation remains the replay
reader's responsibility when a future client actually stores or opens one.

## Worker projection

`GET /api/th03/leaderboard?category=story:rN:cN&format=t3onl2` returns the
strict binary cache representation. The ordinary request remains JSON for
operator/debug use. Entry IDs are SHA-256 digests of a frozen `T3ENT001`
document containing the server-internal stable entry key. Snapshot IDs digest
the fetched category's canonical zero-time/zero-snapshot cache bytes, so fetch
time and local atomic generation do not change content identity. A checked-in
golden vector is encoded by TypeScript and decoded/re-encoded byte-identically
by the native C++ implementation.

## Deferred work

- Define canonical content/ruleset/verifier manifests with the server trust
  model.
- Select and operate a service, verifier queue, authentication, moderation,
  and privacy policy.
- Add bounded HTTP fetch, per-category store naming, cache refresh, and
  downloaded-payload transactions above this boundary.
- Add HiScore/Replay hubs only once real cached or service-backed views exist.
