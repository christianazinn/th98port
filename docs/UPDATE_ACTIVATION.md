# PoDD Port update activation state

`T3ACT001` is the platform-neutral transaction state between authenticated
generation materialization and a future bootstrap launcher. It cannot launch a
process or replace the running executable. Its only path-like value is derived
from a signed `(sequence, artifact SHA-256)` identity as
`PODD-GEN-<sequence>-<first-8-digest-bytes-in-hex>`.

## State machine

The three phases are:

- `stable`: no pending generation; launch the active generation, or the legacy
  installed application when active is absent.
- `pending`: an authenticated generation is nominated but has never launched.
- `trial`: the bootstrap owner durably began exactly one candidate launch.

Nomination requires a sequence greater than every retained identity. The one
exception is an exact retry of `last_failed`; a different artifact cannot reuse
that signed sequence. Beginning a trial increments the state revision and
returns a token containing that revision and full candidate identity. Only an
exact token can acknowledge or fail the trial.

Acknowledgement moves active to previous, pending to active, and returns to
stable. Failure or interrupted-trial recovery clears pending, preserves active,
records the exact failed candidate, increments the bounded failure count, and
returns to stable. A merely pending update is not considered failed after a
restart because no launch was attempted.

A stable active generation that is missing or cannot create a process can be
rolled back only by naming its exact identity. The transition records that
generation as failed, restores `previous` (or the legacy install), and clears
`previous`. If a newer failed candidate already owns `last_failed`, it remains
there so the monotonic sequence history cannot be discarded by a second,
older fallback.

## Fixed file

Each state is exactly 256 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | ASCII `T3ACT001` |
| 8 | 2 | version, exactly 1 |
| 10 | 2 | file size, exactly 256 |
| 12 | 1 | phase: stable 0, pending 1, trial 2 |
| 13 | 1 | presence bits: active, pending, previous, last-failed |
| 14 | 2 | reserved, zero |
| 16 | 8 | monotonically increasing state revision |
| 24 | 8 | greatest signed sequence represented by the state |
| 32 | 4 | failed trial count |
| 36 | 28 | reserved, zero |
| 64 | 40 x 4 | active, pending, previous, and last-failed identities |
| 224 | 32 | SHA-256 of bytes 0..223 |

Each identity is an unsigned 64-bit sequence followed by the complete 32-byte
artifact SHA-256. An absent identity is all zero. Present identities require a
nonzero sequence and nonzero digest, cannot exceed `highest_sequence`, and the
greatest retained sequence must equal that field exactly.

## Durable store

The logical `PODD.UPDATE` path owns two independent files, `PODD.UPDATE.a` and
`PODD.UPDATE.b`. Loading validates both and selects the greatest revision. One
invalid sibling is reported as recovered; no valid sibling is corruption; two
different states with the same revision are ambiguous and fail closed.

A commit obtains a cross-process family lock, re-reads the current state, and
must be exactly one revision newer than it. It writes the inactive sibling
through an exclusive owner-only temporary file, flushes, decodes and compares
it, atomically replaces only that inactive sibling,
flushes the containing directory where supported, and validates the published
file again. The older valid sibling is retained. Exact repeated commits are
idempotent; stale or skipped revisions cannot overwrite newer state.

## Bootstrap transaction

`update_bootstrap` turns the state machine into a launch decision without
adding process or network policy. A bootstrap first acquires the nonblocking
cross-process lifetime lease for the logical `PODD.UPDATE` family and retains
it until its child exits. This distinguishes a durable `trial` left by a dead
bootstrap from one whose child is merely still starting; a second bootstrap
cannot overlap it.

Launch preparation then has exactly three results:

- absent active state selects the legacy installed application;
- stable active state selects its identity-derived immutable generation;
- pending state is committed as `trial` before the candidate generation and
  its exact token are returned.

If preparation finds `trial` after acquiring the lifetime lease, it records
that trial as failed and selects the prior active or legacy target. The failed
candidate is never retried implicitly during the same startup.

The child receives the 107-byte canonical lowercase token text
`T3TRL001:<revision>:<sequence>:<full-artifact-sha256>`. Only an exact token
can acknowledge or fail the durable trial. Exact acknowledgement/failure
retries are idempotent, while a stale child cannot approve a later state.
The lifetime lease and activation-family commit lock are deliberately
different: the child may acknowledge health while its parent retains launch
ownership.

## Executable host

`podd-bootstrap` is the synchronous cross-platform process owner. It resolves
generation executables only beneath the identity-derived immutable directory,
inherits the child's console streams, and retains the lifetime lease until the
child exits. It supplies `TH03_UPDATE_STATE` and `TH03_UPDATE_TRIAL_TOKEN` only
for a trial; ordinary active and legacy children receive neither even if the
parent environment was stale.

Missing files or process-creation failures occur before a child UI can exist,
so a trial is failed or an active generation is rolled back and the recovered
target is launched immediately. Once process creation succeeds, no second UI
is opened during that startup. A trial child that exits without acknowledging
is durably failed for the next launch.

`th03-app` parses the pair as one fail-closed request and acknowledges it only
after settings/content routing has reached a usable SDL platform and input
mapper. This boundary covers the product shell, direct Classic/New Classic,
local VS, and direct netplay launch paths. The standalone always starts the
embedded bootstrap; absent activation state therefore remains exactly the
legacy embedded launch.

## Generation cleanup

The process host runs conservative generation cleanup only after its child has
exited and while it still owns the bootstrap lifetime lease. Cleanup preserves
the full `active`, `pending`, `previous`, and `last_failed` identities, then
the two newest unreferenced complete generations. A directory is eligible only
when it has canonical `PODD-GEN-<sequence>-<digest-prefix>` spelling and a
valid atomic `PODD.GEN` marker binding the same full identity. Links, temporary
trees, malformed names, mismatched markers, unrelated entries, and legacy
unmarked beta generations are ignored. Failure is reported as a warning and
never prevents the already-run application from returning its exit status.

## Deliberately deferred

Production signing pins and visible opt-in update UI remain separate release
gates. The executable host performs no network request, does not nominate a
generation, and cannot activate an inert package by itself.
