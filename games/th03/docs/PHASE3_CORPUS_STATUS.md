# Phase 3 Classic corpus status

Snapshot: **2026-08-09**. All results below invoke `th03-headless` with the
external canonical T3CASE, calibrated `ENEDAT.DAT`, and the frozen Anniversary
`T3SPLIT.BIN` passed explicitly as `--oracle`.

| Case | Current result | Validation evidence / resolved first divergence |
|---|---|---|
| TH3R00 | Green | 715/715 canonical rows and all 100,642 samples. The first hidden divergence had been frame 8,836: CPU-triggered Hyper must fall through to release an already-pending Ellen Charge Shot in the same player update. |
| TH3R01 | Green | 964/964 canonical rows and all 135,215 samples. The first hidden divergence had been global frame 19,326: Rikako Boss state 6 must dispatch to the RNG-driven four-spread pattern, consuming angle/type draws and adding eight bullets. |
| TH3R02 | Green | 626/626 canonical rows and all 91,939 samples. The first hidden divergence had been frame 38,336: Kana Extra writes the shared bullet-template angle before Kana Boss intentionally inherits it later in the same prefix. |
| TH3R03 | Green | 186/186 canonical rows and all 25,211 samples. |
| TH3R04 | Green | 627/627 canonical rows and all 86,437 samples. |
| TH3R05 | Green | 783/783 canonical rows and all 111,747 samples. |

TH3R00 and TH3R02 cover their complete Story and process lifecycles; TH3R02
includes its terminal Continue-request boundary. TH3R04 and TH3R05 cover
route-1 repeat rounds. Their repeat-round mismatch was
resolved by restoring the exact `sub_9778()` HUD-start schedule: the HUD makes
`enemy_formations_update()` return, but bullets, existing enemies, fireballs,
and both players' gauge-pattern callbacks still update. The old native prefix
returned early from all of those systems. The headless coordinator now also
accepts terminal MAINL boundaries that reach the typed ending, exit, or
Continue-request application seam without inventing another gameplay process.

TH3R01 closes the remaining character-dispatch integration gap. Anniversary's
Rikako jump table maps states 2-5 to spreads, 7-12 to Extra Attacks/rings, and
6 plus 13-17 to four-spreads. The native switch had shifted boundary states
6, 7, and 13 into the neighboring families. A focused dispatch test now locks
the exact state-6/7/13 behavior and RNG schedule.

This table records validation coverage, not implementation scope. The final
whole-source audit independently covers every stock-reachable gameplay
callback, lifecycle path, renderer-fed deterministic side effect, and stock
ENEDAT operator; see `PARITY_SOURCE_COVERAGE.md`. Future corpus additions
remain valuable integration evidence and must not be used to redefine that
implementation scope.

## Fresh Demo Gate A

The fresh current Master/Anniversary no-input Demo case is green for all 59
schema-3 rows and all 7,827 samples through terminal route 0. The frozen case
SHA-256 is
`816A6574035E2A18C86F6EBCF66DF084E2EB0C379417E93A2DE9A9F706A1CB91`; the
Master split SHA-256 is
`8FFCC69D33C1B9647F95E7ED4524D68EB5F1A89D17A22D97E871B68576011B1E`.

Before the correction, the canonical states were exact through frame 261 and
first differed in P1's X coordinate at frame 262. Anniversary's P1 CPU broad
probe wraps the packed collision-map pointer to DGROUP `0x0351..0x0352`, where
the public-domain master.lib sine table contains little-endian bytes `00 AC`.
The active `0x0F` mask therefore reports a collision at `(0, 0x0CA0)` and picks
the rightward avoidance route. Native now reconstructs the linked sine/cosine
table bytes from its integer trig functions; focused tests lock the narrower
misses and the 80-tile hit without embedding a memory snapshot.

## Historical replay integration gate

The direct `th03rv` gate additionally verifies 19 locked V11/V12 replay files
and all 173 embedded round summaries. It covers the six V11 Story runs above,
thirteen files from the V12 directory, Replay Patch Practice, and ordinary VS.
The exact file identities and per-file results are recorded in
`REPLAY_CORPUS_LOCK.tsv` and `REPLAY_VERIFICATION.md`.
