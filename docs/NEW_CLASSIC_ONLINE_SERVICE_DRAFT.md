# New Classic online leaderboard and replay service

Status: **local foundation implemented; production adapter, deployment, and
client projection pending**

Decision date: 2026-08-15

The first service should be deliberately small. A client identifies itself,
uploads one V13 replay, the server runs one pinned `th03rv` verification, and
an accepted result becomes an online leaderboard/replay row. The online screen
should look and behave like the existing Replay browser, with score ordering
and online ownership added rather than a separate web-style UI language.

## Product policy

- The visible product calls these ordinary online leaderboards. It does not
  display an explanatory `Replay Verified` or `Open` badge.
- Users may treat the boards as RTA under the community honor system.
- Internally, the server's actual proof remains deterministic replay validity:
  it does not establish human authorship, an unmodified Windows client, or
  real-time execution.
- Stock New Classic Story is the first uploadable mode.
- The main board shows only the best entry per backend user profile and
  category. The profile's other accepted records remain available in `My
  Records`.
- Higher score ranks first. Equal scores rank by oldest accepted record first.
- Historical pre-Replay Patch records are manually inserted into the same
  board. They were reviewed against video evidence but do not need a public
  evidence link and have no downloadable replay action.
- Every normal accepted entry links to its exact downloadable replay.
- Autofire and the replay's recorded slowdown are visible in replay details in
  the same style as existing metadata.
- A replay whose recorded slowdown is greater than 5% is ineligible and must
  be rejected before publication.

Slowdown is the exact value written by the local recording path. The server
checks the encoded value and threshold but cannot independently reproduce the
original machine's wall-clock performance. That is an accepted limitation of
the chosen small honor-system model.

## Minimal trust statement

Building or uploading the replay from memory is useful and avoids unnecessary
temporary files, but it is not an anti-cheat boundary. A modified Windows
client can alter memory, code, or the request body. The v1 server therefore
does only what it can prove simply and deterministically:

1. Strictly parse the uploaded V13 file.
2. Run the complete input stream through the pinned stock verifier.
3. Rebuild checkpoint zero from a snapshot-free stock Story launch, compare
   its complete first-round observation, then require every stored checkpoint,
   split, and terminal checked by `th03rv` to match simulation.
4. Require the encoded slowdown to be at most 5%.
5. Publish the server-derived row only after those checks pass.

There is no speculative anti-cheat, heuristic flagging, executable attestation,
or claim that file storage versus memory storage prevents cheating.

## Small Cloudflare shape

```text
native client
  -> Worker endpoint
  -> pinned native th03rv verifier
  -> accepted replay bytes in R2
  -> accepted row in D1
  -> leaderboard/replay read endpoints
```

The existing site is deployed with `npx wrangler deploy` from
`tools/azinn-site-v2` in the retained replay-validation worktree. The service
should extend that Worker project rather than create an unrelated deployment.

Recommended bindings:

| Owner | Purpose |
|---|---|
| Worker | user enrollment, authenticated upload, fixed validation gate, read API |
| Cloudflare Container | pinned native `th03rv` invocation if current account support and limits are suitable |
| R2 | exact accepted V13 bytes keyed by replay SHA-256 |
| D1 | users, accepted entries, historical entries, and best-per-profile queries |

No Queue is required for v1. Verification should run synchronously behind one
bounded service call if measured replay-verification time and Container startup
fit the deployed request path. If they do not, the same endpoint may later
return a submission ID and use one Queue without changing replay, user, or
leaderboard schemas.

Current primary platform references:

- [Cloudflare Containers](https://developers.cloudflare.com/containers/)
- [D1 transactional batches](https://developers.cloudflare.com/d1/worker-api/d1-database/#batch)
- [R2 consistency](https://developers.cloudflare.com/r2/reference/consistency/)

## Identity and authentication

The backend issues one opaque random user ID and one random secret token. The
client stores both outside `PODD.CFG`; D1 stores the user ID and only a salted
token hash. The token authenticates uploads and `My Records`. There is no
email, OAuth, passkey, public account page, rename system, or recovery flow in
v1.

The user ID is backend ownership, not display identity. Leaderboard and replay
rows display the eight-character name encoded in that replay. Historical rows
display the curator-entered name.

This one secret is the minimum idiomatic protection against submitting records
under another user's ID. It is not presented as strong client attestation.

## API

The initial API can remain five routes:

```text
POST /api/th03/users
POST /api/th03/replays
GET  /api/th03/leaderboard?category=...
GET  /api/th03/users/{user-id}/records
GET  /api/th03/replays/{replay-id}
```

`POST /users` returns the new user ID and token once. `POST /replays` takes an
authorization token and raw replay bytes; filenames, client score fields,
split fields, and result fields are not authoritative request inputs.

Public outcomes are intentionally only:

```text
accepted
rejected
error
```

- `accepted`: deterministic checks and eligibility passed; response includes
  the published row/replay ID.
- `rejected`: the replay is invalid, mismatched, unsupported, or over 5%
  slowdown; response includes one bounded player-facing reason.
- `error`: infrastructure/verifier failure; retry may succeed.

Internally logged details may be more precise for diagnosis, but do not create
a public verdict taxonomy.

## Data model

### `users`

```text
user_id TEXT PRIMARY KEY
token_hash BLOB NOT NULL
created_at INTEGER NOT NULL
```

### `replays`

```text
replay_id TEXT PRIMARY KEY       -- uppercase SHA-256 of exact accepted bytes
object_key TEXT UNIQUE NOT NULL
byte_count INTEGER NOT NULL
name TEXT NOT NULL               -- from replay metadata
mode/rank/character/category
score/end_stage/result
autofire INTEGER NOT NULL
slowdown_percent INTEGER NOT NULL
accepted_at INTEGER NOT NULL
```

### `entries`

```text
entry_id TEXT PRIMARY KEY        -- server-internal UUID in this foundation
profile_key TEXT NOT NULL        -- user:<id> or stable history:<person>
user_id TEXT NULL                -- NULL only for curated historical rows
replay_id TEXT NULL              -- NULL only for historical rows
source_kind TEXT NOT NULL        -- replay or historical
display_name TEXT NOT NULL
category fields
score/result fields
accepted_at INTEGER NOT NULL
```

The main-board query partitions every row by `(profile_key, category)`, chooses
score descending then accepted time ascending within that profile, and orders
those winners by the same score/oldest-tie rule. Historical rows use stable
`history:<person>` keys, do not need a backend user ID, and disable replay
watch/download actions.

## Replay bytes and local recording

The live native recorder remains an in-memory document authority. Online
submission can encode the finalized V13 document to a byte vector and send
those bytes directly, avoiding a temporary numbered replay file.

Keep the existing `TH3LAST.RPY` current-process journal. It protects the active
recording transaction from intermediate storage failure and its removal would
not make memory or network submission harder to tamper with. It is not a
cross-launch recovery queue: a fresh application start discards an interrupted
journal before the title. An ordinary saved/downloaded replay remains a normal
disk file and can be uploaded later from the Replay menu.

R2 stores candidate accepted bytes under their digest. Payloads rejected before
that put are not retained. Before D1 publication, read back the object and
confirm its size and SHA-256. If D1 publication then fails, leave the
content-addressed object as an unpublished orphan: inline deletion could race
an accepted concurrent upload of the same digest. Garbage collection is a
separate reachability pass against a consistent D1 snapshot.

## Required safety without product complexity

These are fixed parser boundaries, not a quota product:

- accept raw V13 bytes only;
- enforce one conservative maximum byte count and verifier wall-time;
- never accept client paths, URLs, archives, content files, or executables;
- use prepared D1 statements and bounded strings;
- compare SHA-256 before and after R2 publication;
- invoke a fixed verifier binary/content path with no user-generated command
  line; and
- redact the secret token and replay bytes from logs.

No per-day upload quotas, moderation workflow, reports system, proof manifests,
queue lease state machine, or account platform is required for v1. Add one only
after an observed operational need.

## Client presentation

The first online leaderboard reuses the Replay browser's proportional rows,
detail page, tabs, and metadata vocabulary. It adds rank/order and online
availability without inventing a host-styled surface.

- Normal row: name, score, character, rank, stage/result; detail includes
  Autofire, slowdown, splits, and replay watch/download.
- Historical row: the same score row, no replay action, no required evidence
  link.
- `My Records`: all accepted rows owned by the hidden backend user ID.
- The main board: one best row per backend profile/category plus historical
  rows, with older equal scores first.

## Implementation order

1. Add structured machine-readable output to `th03rv` with only
   `accepted/rejected/error`, server-derived score/category, exact split
   agreement, Autofire, and slowdown.
2. Audit the existing V13 slowdown encoding and freeze literal `>5%`
   rejection by exact cross multiplication while retaining the local replay
   viewer's floored display integer.
3. Inspect and extend `tools/azinn-site-v2`; bind local D1/R2/Container
   development without deploying production data first.
4. Add the small D1 schema and fixed endpoints with a fake verifier, then swap
   in the pinned native verifier.
5. Add in-memory finalized replay upload and the online Replay-style views.
6. Seed reviewed historical records directly into D1 and run one private beta
   before public upload.

## Implemented local foundation (session 194-online)

The canonical local foundation lives under `tools/azinn-site-v2/service`.
It supplies the D1 schema, content-addressed R2 publication, opaque
ID/salted-token
authentication, five routes, best-per-profile query, historical profile rows,
and fail-closed pinned verifier fetcher seam. It has no production IDs,
credentials, or verifier bypass and has not been deployed.

`th03rv --service-json` now supplies the typed `T3RV-SERVICE-1` inner report.
An accepted report derives visible score (including TH03's trailing-zero score
unit), terminal stage, clear/Game Over kind, and continues used from simulated
or reconstructed state rather than trusting V13 header result fields. The
future pinned Container wrapper adds only the exact replay SHA-256 and the
fixed content/ruleset/verifier identities; the Worker requires all pins to
match. See the service README for the exact adapter boundary.

Slowdown eligibility is frozen to the literal ratio using overflow-safe exact
cross multiplication: `slow_frames * 100 <= timed_frames * 5`. Zero timed
frames are ineligible. The UI still displays
`floor(100 * slow_frames / timed_frames)`; 5.00% is eligible and 5.01% is not.

The Worker incrementally caps raw upload bodies at 1 MiB, incrementally caps
verifier replies at 16 KiB, and aborts the verifier call after 15 seconds. The
pinned Container wrapper must also kill its fixed native process within a
shorter deadline.

This foundation is not yet a client-ready projection for the frozen
`T3ONL001` cache. That cache additionally requires digest entry/snapshot IDs,
pinned identity and DOS-date fields, replay availability, and a revised
curated-history invariant compatible with records that intentionally expose no
evidence link. Freeze that response mapping before wiring the native client;
do not treat the current server-internal UUID/JSON shape as `T3ONL001`.
