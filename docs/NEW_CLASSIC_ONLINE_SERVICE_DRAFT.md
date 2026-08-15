# New Classic online service and verifier draft

Status: **proposed for iteration; no service has been deployed**

Draft date: 2026-08-15

This document turns the frozen client-side identities and `T3ONL001` cache
boundary into a deployable service shape. It deliberately keeps the protocol
independent of Cloudflare even though the first deployment is expected to use
a Cloudflare Worker.

## Recommended launch promise

The first public boards should be described as **replay-verified/open**, not
human-only or real-time-attack boards. A deterministic verifier proves that a
V13 input stream legally reaches a result under a pinned build and ruleset. It
cannot prove who generated the inputs, whether a tool or macro was used, or
whether the original play session ran in real time.

Recommended first scope:

- stock New Classic Story only;
- one pinned content manifest, ruleset, and verifier image;
- anonymous reads and account/invite-authorized publishing;
- exact replay download required for every published verified score;
- best score per public profile on the main board, with all accepted runs in
  profile history;
- curated pre-Replay Patch records in `Historical Records`, not silently mixed
  into the verified board; and
- Autofire as a visible filter/facet, never mixed invisibly. Slowdown remains
  non-authoritative replay detail because a server replay cannot verify the
  uploader's wall-clock host performance.

Practice, VS, Arrange rules, human/RTA moderation, and cross-ruleset rankings
should wait until each has an explicit board policy.

## Trust boundary

```text
untrusted native client
  -> edge API: auth, quotas, strict byte gate, SHA-256, durable request
  -> exact replay object + verification job
  -> at-least-once queue carrying IDs only
  -> isolated pinned native th03rv runner
  -> structured simulation-derived result
  -> idempotent publication transaction
  -> anonymous read API / immutable replay download / offline cache
```

The client never supplies an authoritative score, terminal result, category,
content file, executable, verifier verdict, or storage path. Client metadata is
only a submission hint. The verifier selects server-owned allowlisted content,
parses the exact replay bytes, reconstructs every checkpoint, runs the complete
input stream, and derives the publishable outcome from simulation.

The runner must be unprivileged and resource-limited, with a read-only image and
content set, an empty per-job directory, no credentials, bounded input/sample/
CPU/wall/memory/output, and process-tree termination. It accepts object and
manifest IDs, never a client URL or shell fragment.

## Cloudflare mapping

The current platform supports this split without making it part of the wire
protocol:

| Concern | First deployment owner |
|---|---|
| `/v1` request/auth/read API | Worker |
| Exact V13 objects and bounded private diagnostics | R2 |
| Submission, lease, result, entry, moderation, and idempotency rows | D1 |
| At-least-once verification wake-ups | Queue |
| Pinned native `th03rv` process | Cloudflare Container if its operational isolation is sufficient; otherwise a minimal external container pull-consumer |

Cloudflare Queues documents at-least-once, unordered delivery, so the durable
D1 row is the job state machine and queue delivery is only a wake-up. D1 unique
constraints and transactional batches enforce deduplication and atomic result
publication. R2's direct bindings are strongly consistent; immutable digest
keys avoid cached-overwrite ambiguity.

Cloudflare Containers can currently run arbitrary `linux/amd64` images behind
a Worker and Durable Object. That makes a native verifier plausible, but the
compute adapter remains open until the actual account, limits, cold-start cost,
outbound-network policy, and per-process isolation are inspected. An external
runner can consume the same queue contract without changing any public API.

Current primary references:

- [Cloudflare Containers](https://developers.cloudflare.com/containers/)
- [Queues delivery guarantees](https://developers.cloudflare.com/queues/reference/delivery-guarantees/)
- [Queues batching, retries, and dead letters](https://developers.cloudflare.com/queues/configuration/batching-retries/)
- [D1 transactional batches](https://developers.cloudflare.com/d1/worker-api/d1-database/#batch)
- [R2 consistency](https://developers.cloudflare.com/r2/reference/consistency/)

## Immutable identities

Use canonical binary manifests for authority and JSON only as an API/operator
projection:

| ID | Canonical input |
|---|---|
| `ReplayId` | exact strict-valid V13 bytes, including optional accelerator tail |
| `ContentId` | `T3CNT001`: sorted gameplay-content role/name/size/SHA-256 records |
| `RulesetId` | `T3RUL001`: deterministic rules ABI and accepted input/options |
| `VerifierId` | `T3VER001`: source, exact binary/image, toolchain, schema, sandbox and acceptance-corpus digests |
| `BoardId` | `T3BRD001`: eligibility, ranking, tie, Autofire, publication, and legacy policy |
| `VerificationResultId` | `T3VFY001`: four proof IDs, derived outcome, counters, verdict, and schema |
| `EntryId` | `T3ENT001`: board, proof/evidence, result, public profile, and publication nonce |
| `SnapshotId` | `T3SNP001`: query, board/revision, ordered entries/replays, cursor, time, and tombstones |

The allowlist, not possession of a digest, grants authority. V13 does not embed
content or ruleset identities, so the stock server selects and records them.
Arrange should use a future replay format that binds those IDs.

`ReplayId` intentionally includes accelerator bytes. Public download must
return the exact submitted bytes. A semantic input-stream ID, if ever useful,
would be a separate identity.

## Structured verifier contract

Do not scrape the current human-readable `th03rv` stdout and do not publish
header `final_score` as truth. Add a library boundary or sibling service-mode
executable whose success result derives:

- replay digest and exact byte count after strict V13 parsing;
- content, ruleset, verifier, and requested board IDs;
- mode, rank, characters, Practice coordinates, Key Mode, and Autofire;
- terminal kind/stage/round/winner, exact numeric score, and continues used;
- consumed sample count, checkpoint and split agreement, and completion state;
- bounded stable verdict/rejection code and result digest.

The replay header remains comparison evidence. The replay runner needs to
retain its typed final transition (or equivalent outcome) so score and terminal
state come from simulation. Service verdict classes should be stable and small:

```text
accepted
format_rejected
content_rejected
simulation_mismatch
policy_rejected
resource_limit
verifier_fault
```

Only deterministic final verdicts are reusable forever. Resource and verifier
faults are retryable.

## Submission state and idempotency

```text
received -> object_committed -> queued -> verifying
         -> accepted -> published | unlisted
         -> rejected
         -> retry_wait -> queued
         -> quarantined
```

- Scope `Idempotency-Key` to the publishing principal and retain the body
  digest plus original response. Reuse with different bytes is `409`.
- Store an exact replay once by `ReplayId`, with put-if-absent and read-back
  digest/size validation.
- Make verification unique on
  `(ReplayId, ContentId, RulesetId, VerifierId)`.
- Claim work with a lease, attempt, and lease generation; complete with a
  compare-and-set operation.
- Make result insertion and entry publication independently idempotent.
- Route exhausted retries to a dead-letter/quarantine path; never infer job
  truth from the queue itself.

## Minimal API

```text
GET    /v1/capabilities
POST   /v1/submissions
GET    /v1/submissions/{id}
DELETE /v1/submissions/{id}
GET    /v1/leaderboards/{board-id}?category=...&cursor=...
GET    /v1/replays/{replay-id}
GET    /v1/replays/{replay-id}/download
GET    /v1/me/submissions
POST   /v1/reports
```

Uploads are raw V13 only: no zip, content encoding, URL import, executable, or
user filename. Immutable replays/manifests use digest ETags and long caching;
leaderboard snapshots use bounded freshness and conditional refresh.

## Identity, privacy, and moderation

Reads remain anonymous. Recommended private beta is invite-token publishing
with a secret management token; the later public flow pairs the game through a
browser to a passkey, OAuth, or magic-link account and stores only a scoped
rotatable credential in the OS secret store. `PODD.CFG` never stores secrets.

Publishing is explicit and off by default. The consent text must say that the
exact replay exposes its eight-character name, packed date, and complete input
stream. Public profiles use opaque IDs and chosen display names; email/OAuth
identity, IP data, and security logs remain private and retention-bounded.

Withdrawal unlists an entry and its download. Physical deletion occurs only
after a grace period and when no other publication, audit, or legal reference
owns the deduplicated object. Curated legacy records require logged operator
review and explicit evidence; they never receive a fake replay or verifier ID.

## Client/offline evolution

The frozen `T3ONL001` codec remains valid as one bounded snapshot, but one
256-record `PODD.ONL` cannot own every rank/character/query. Before visible
hubs, add:

- a small query/index document;
- one content-addressed `T3ONL001` snapshot file per query;
- atomic full-query replacement;
- ETag refresh with stale/current/offline/service-unavailable states; and
- merge of local `downloaded` flags only after exact ReplayId/size validation.

Downloads publish atomically under `<REPLAYID>.RPY` and never overwrite local
numbered slots. Service signatures for offline-authenticated snapshots are a
useful later hardening step, not a private-beta blocker.

## No-regret implementation order

1. Add structured simulation-derived verifier output and hostile/resource
   limit tests locally.
2. Encode and reproducibly generate the five authority manifests through
   `T3VFY001` on GCC and MSVC.
3. Decide launch board promise, Story scope, Autofire display, replay-public
   consent, and beta identity.
4. Extend the cache/domain with BoardId, policy, Autofire, opaque profile, and
   per-query ownership.
5. Implement a completely local fake edge/object/database/queue/runner stack.
6. After Cloudflare onboarding, bind that tested contract to Worker, D1, R2,
   Queue, and the selected container adapter.
7. Ship read-only/cached hubs before enabling public upload if operational
   confidence warrants it.

## Decisions to iterate with the product owner

1. Confirm `Replay Verified`/open rather than human/RTA as the launch promise.
2. Confirm Story-only first.
3. Confirm Autofire as a visible filter and no slowdown eligibility gate.
4. Confirm every published verified score has a public exact replay.
5. Choose invite-token beta, then passkey/OAuth/magic-link public identity.
6. Confirm best-per-profile main ranking, deterministic tie order, and
   Historical-only legacy records initially.
7. Decide whether offline snapshot signatures are needed before public launch.
8. Select Cloudflare Container versus external pull runner after account and
   isolation inspection.
