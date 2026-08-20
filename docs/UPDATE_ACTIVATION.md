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

## Deliberately deferred

An executable host still needs to resolve the selected identity beneath its
generation root, launch the child, transport the canonical token, define the
early application health boundary, and retain or clean immutable generations.
Production signing pins and visible opt-in update UI also remain separate
release gates.
