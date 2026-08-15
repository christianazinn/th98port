# New Classic online identity and offline cache

Status: client-side domain boundary frozen in session 189. No service or
visible online route is implemented.

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
- a curated historical score must name its evidence and curator note and must
  not contain a replay or verifier identity; and
- downloaded replay filenames are content-addressed local names, never remote
  paths.

## Identities

Every identity is a strongly typed 32-byte SHA-256 digest. Types cannot be
mixed at the C++ API boundary.

| Type | Canonical digest input |
|---|---|
| Replay | Exact validated V13 file bytes |
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
and verifier identities plus nonempty evidence and curator text. When cached
replay metadata is present for a verified entry, its content, ruleset,
category, and result must match the leaderboard entry exactly.

All visible strings are bounded, control-free UTF-8. The document is limited
to 256 leaderboard entries, 256 replay records, and 256 KiB.

## `T3ONL001` cache format

All integers are little-endian. The header is 64 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `T3ONL001` |
| 8 | 2 | version `1` |
| 10 | 2 | header size `64` |
| 12 | 4 | total file size |
| 16 | 4 | local atomic generation |
| 20 | 2 | leaderboard-entry count |
| 22 | 2 | replay-record count |
| 24 | 8 | fetch time, Unix seconds |
| 32 | 32 | snapshot identity |

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
u16+bytes title
```

An empty document has no snapshot/fetch identity and is the normal result when
no cache file exists. A nonempty document requires both. `PODD.ONL`, `.tmp`,
and `.old` are a fixed atomic recovery family; the greatest valid generation
wins. A present but wholly corrupt family returns an error rather than being
silently reinterpreted as online data.

Downloaded payloads use `<64 uppercase replay-id hex digits>.RPY`. The cache
only records metadata and availability; V13 validation remains the replay
reader's responsibility when a future client actually stores or opens one.

## Deferred work

- Define canonical content/ruleset/verifier manifests with the server trust
  model.
- Select and operate a service, verifier queue, authentication, moderation,
  and privacy policy.
- Add service/cache merge and downloaded-payload transactions above this
  boundary.
- Add HiScore/Replay hubs only once real cached or service-backed views exist.
