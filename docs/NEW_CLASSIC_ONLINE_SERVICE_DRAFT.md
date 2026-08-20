# New Classic online leaderboard and replay service

Status: **production Cloudflare verifier/storage/read path live; official
Windows client supports background boards, in-memory replay upload, My Records,
download, and Watch; reviewed historical seeding and non-Windows account
mutation remain**

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

The existing site is deployed with `npx wrangler deploy` from the canonical
`tools/azinn-site-v2` checkout. The service extends that Worker rather than
creating an unrelated deployment.

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

Active native recording is memory-only. Finalization produces a canonical V13
byte vector in memory, which can be submitted directly without creating a
temporary replay file. A local save begins disk I/O only after a numbered slot
is confirmed and atomically publishes `TH3Rxx.RPY`; saved and downloaded files
can still be uploaded later from Replay. Fresh launches remove `TH3LAST.RPY`
only as migration cleanup for older native builds.

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
6. Seed reviewed historical records directly into D1 after the reviewed record
   dataset is supplied. Public replay upload is already live.

## Implemented service and client path

The canonical service foundation lives under `tools/azinn-site-v2/service`.
It supplies the D1 schema, content-addressed R2 publication, opaque
ID/salted-token
authentication, five routes, best-per-profile query, historical profile rows,
and a fail-closed pinned verifier fetcher seam. Production identities remain
deployment configuration rather than source defaults; there is no verifier
bypass.

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

Session 198 adds the explicit client projection. The ordinary leaderboard JSON
remains an operator surface; `format=t3onl2` joins the pinned replay facts and
returns the strict `T3ONL002` binary cache. It derives stable typed entry and
snapshot SHA-256 identities, binds the response to one category, preserves DOS
date/payload availability, and carries exact Autofire/timed/slow frames. A
cross-language golden vector proves Worker/native byte compatibility.

Session 199 adds the platform-neutral transaction layer. It requests exactly
one bounded binary board and commits it to a dedicated rank/character cache,
preserving a local monotonic generation. Enrollment accepts only the service's
two credential strings. Upload serializes the finalized V13 directly into
request memory and never writes a candidate journal or temporary replay.
Download verifies the content digest, V13 parse, and every available cached
metadata field before atomically preserving the exact response bytes (including
optional accelerator tails) under the online replay directory. Later sessions
add the visible owner, host tasks, official origin, and production deployment.

Session 200 supplies the Windows boundary. WinHTTP is fixed to one
explicit HTTPS origin, rejects redirects and cross-origin/absolute targets,
bounds connect/request time and reply bytes, and exposes no platform state to
gameplay. DPAPI protects the opaque user ID and token together for the current
Windows account through an atomic read-back-validated store. Official builds
use `https://azinn.dev`; `TH03_ONLINE_ORIGIN` can override it for staging or
disable online work when explicitly empty.

Session 201 adds the first visible offline-first owner. If at least one valid
T3ONL002 Story board exists for the active rank, HiScore opens a source-styled
hub with the existing local viewer and a Replay-style cached online board. The
board exposes player, score, character, rank, result, date, Autofire, slowdown,
and replay availability, including ordinary historical entries without fake
replay actions. Cache files are loaded before deterministic session ownership;
the menu itself is immutable and never performs filesystem or network work.
With no valid active-rank cache, the direct local HiScore route remains exactly
as before.

Session 202 adds the first asynchronous production transaction owner. The
official origin enables refresh by default, while Classic, replay capture, and
pixel-oracle runs never start it. The Windows worker sequentially refreshes
the nine Story boards for the configured rank through the existing bounded
HTTPS transport. Each response is validated and atomically cached off the
source thread; complete immutable documents are merged only between host-loop
iterations and become visible on the next HiScore entry. Network, cache, and
allocator failures are warnings and cannot interrupt local play. The transport
outlives and is joined after its worker, and unexpected worker exceptions fail
closed instead of escaping the thread.

Session 212 adds the first explicit replay action. Confirm on a replay-backed
online HiScore detail emits one immutable request through the OP session and
runner; a separate one-shot worker performs the existing bounded download.
Only exact digest, V13, cached metadata, and atomic content-addressed
publication success changes the UI to `Downloaded`. Missing endpoints report
`Offline`, other failures remain retryable, and neither path blocks a source
tick. On later launches the same checks revalidate local bytes before restoring
downloaded availability.

Session 213 completes the local Watch half of that action. Confirm on a
downloaded replay emits a separate immutable intent; the host re-reads and
validates ReplayId, V13 structure, and cached metadata before handing the
document back to OP. The session stops HiScore music, publishes one black
source boundary, adopts the existing checkpoint-zero playback owner, and asks
the host scheduler to rebase before the first gameplay tick. A stale local file
is demoted to a retryable download and never reaches playback.

Session 214 exposes the replay-oriented side of the same immutable cache. If
the active rank contains any replay records, title Replay opens a proportional
four-row hub for Local Replays, Online Replay Library, Downloaded Replays, and
Return. The two library modes contain replay-backed rows only, use replay names
instead of account display names, and reuse the exact Download/Watch host
transactions above. With no replay-backed cache, Replay still enters the local
browser directly.

Session 258 exposes explicit upload without changing the deterministic owner.
On Windows, the official or overridden origin adds one Upload Replay row even when no cached
board contains records. Confirming a numbered local slot emits one immutable
parsed V13 document; a host worker loads or enrolls the DPAPI-protected opaque
identity and passes the document to the existing in-memory upload transaction.
The only replay bytes created are the bounded HTTPS request body. A result is
latched into the retained screen on the next source boundary, while an absent
origin or unsupported platform exposes no inert row.

Session 259 adds `My Records`: the authenticated history route projects each
exact Story category as the existing bounded T3ONL002 document. The native
client acquires one existing identity off source time, keeps the private
documents in memory, and reuses Replay Download/Watch. Startup never enrolls
merely because an origin exists; the row appears only for nonempty accepted
history loaded from an existing identity or refreshed after a successful
explicit upload. Selecting Upload Replay is the explicit upload consent action;
first use may enroll the opaque identity automatically.

Session 302 closes production acceptance. The private `podd-th03-verifier`
Container runs the pinned native verifier and the public Worker pins its exact
content, ruleset, and verifier identities. The supplied genuine WR
`TH3R06.RPY` (SHA-256
`464BE1B6FB68AF04C598C1B5E9B55D033ACF8FB6660C6978118E2DF9ADCF99D3`)
was accepted at `https://azinn.dev`, projected into `T3ONL002`, returned through
My Records, and downloaded from R2 with all 213,592 bytes unchanged. The
service-derived row is CZZ / Mima / Lunatic / 152,255,320 / Clear, with
Autofire and 730 slowdown frames out of 140,197 timed frames. The verifier fix
removed a false equality between the pre-initialization resident seed and the
recorder's post-initialization live seed; snapshot-free stock reconstruction,
the full first-round observation, checkpoints, splits, terminal state, and the
literal slowdown threshold remain enforced.
