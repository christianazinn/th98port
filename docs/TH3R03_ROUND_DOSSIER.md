# TH3R03 first-round donor dossier

status: implementation prerequisite frozen

## Evidence identity

- case: `TH3R03.T3C`
- case SHA-256:
  `62b28fc4910abac56cf5677ad56bf8306292b8a4027775a85703cdb0b0485ef8`
- Anniversary split SHA-256:
  `533339941141a0afcb2d2599234b2bf4b03e17abe7ee7a92340d48bf9691fcc3`
- gameplay donor: Anniversary
  `3e2381d4275be7287f5525de5c95389d492b0a3c`
- historical tie-breaker: Master
  `54cb12d0f888304b923fa1c78158d1f12fca3d45`
- Replay equivalence reference:
  `75387f747b8c146f4d94c559dc4f624bf9082f94`

The evidence files remain ignored in the Phase 0 harness worktree. This
document records only identities and derived facts; it contains no replay or
game data.

## Scenario

TH3R03 starts in Story mode, Stage 1, Lunatic, with human Reimu palette 0
against CPU Kana palette 0. P1 Autofire is enabled. The V11 snapshot begins
with both players at raw Q12.4 `(2304, 5376)`, 10 half-hearts, 50
invincibility frames, 32 raw gauge-charge speed, 0 charged gauge, 1024
available gauge, 2 bombs, enabled shots, and CPU frame 0.

The first MAIN process naturally chooses route 2 after 6,773 logical input
samples. Of these, 5,504 are gameplay samples and 1,269 are interstitial
warning-loop samples. The gameplay loop ends at `round_frame == 5503` and
`round_or_result_frame == 336`. The oracle prefix contains 46 rows: one
`start`, 43 gameplay checkpoints, one `route`, and one `finish`. The final
scores are P1 `01090503` and P2 `00434365`; P1 wins.

The first interstitial wait begins after 1,433 gameplay samples. There are 47
interstitial runs, each 27 samples long. Interstitial samples do not advance
either gameplay counter, but are part of exact replay-input consumption.

## Initialization order

The native translation preserves this donor order:

1. `round_startup()` sets `random_seed = resident->rand`, loads formations,
   sets `round_id = 0`, and calls `sub_9EBF()`.
2. `sub_9EBF()` resets round subsystems, fills `randring`, initializes the two
   players and their 64-byte CPU charge rings, chooses Lunatic round values,
   and randomizes formations.
3. `round_startup()` initializes Story score/difficulty, round-win and combo
   maxima, input mode, character callbacks, and presentation resources.
4. The V11 adapter restores only the audited runtime snapshot after ordinary
   initialization. It restores RNG/rings/cursors and the documented player
   subset; it does not restore round carry state.
5. The `start` split is emitted before the first logical gameplay sample.

Hardware/resource/presentation operations in these functions are not core
state. Their call positions remain represented where they can affect gameplay
through callbacks or RNG.

## Gameplay frame order

The authoritative order is `th03/main/rndloop.cpp:sub_9778()` at the pinned
Anniversary commit:

1. collision map and bullet-template reset;
2. P1 then P2 Extra Attack, Boss Attack, and character callbacks;
3. hit circles, player shots, P1/P2 charge shots;
4. enemy formations, bullets, enemies, and fireballs;
5. P1 then P2 gauge-attack patterns;
6. input sense, replay Autofire completion, P1 then P2 player update;
7. pause/warning and per-player post-update work;
8. background callback;
9. gameplay-relevant work historically interleaved with rendering, including
   fireball hit-testing and combo updates;
10. defeat/result state, round callbacks, and frame counters.

Do not reorder nominally independent P1/P2 calls: they share globals and RNG.

## Canonical state-hash schema 3

The hash starts at 5381. Each byte applies `hash = hash * 33 XOR byte`, with
multi-byte values added little-endian. The exact field order is:

1. `round_frame`, `round_or_result_frame`, `round_speed`, `defeat_flag`,
   winner;
2. P1/P2/system input latches;
3. both eight-digit scores in least-significant-digit-first storage order;
4. both players: center X/Y, half-hearts, invincibility, shot mode,
   knockback/move-lock, charged/available gauge, bombs, rounds won, CPU frame,
   next hit damage, shot state, CPU dodge strategy, last human movement,
   spell-ready frames, combo maxima, and attack counters;
5. `randring` cursor, the two Autofire shot gates, bullet-add scratch,
   per-player boss-panic flags, normalized background phase, and normalized
   result phase;
6. both combo records, with only the presentation-only displayed hit count
   clamped to 99;
7. chain cursors and all four 2x16 chain arrays in declaration order; and
8. live 32-bit RNG seed.

Pointers, entity arrays, padding, renderer/audio state, host state, and replay
cursors are excluded. They still must be simulated whenever they feed any
hashed field or future gameplay.

## Anniversary-to-Replays audit boundary

The full `th03/main` area contains Replay instrumentation and QoL changes in
addition to the oracle module. Phase 2 ports Anniversary gameplay only. Every
modified pre-existing gameplay file is reviewed before its subsystem is
translated; Replay-only file I/O/UI code is not a donor. Known comparator
normalizations are limited to the documented three-digit combo presentation
delta and terminal post-route input residue. No gameplay delta is accepted or
hidden.

## Phase 2 gate

The first-round candidate must be produced from `GameInit` plus the logical
input stream. A green result requires all 46 Anniversary rows through the first
`finish`, exact first-process input consumption (6,773 samples plus its MAIN
control record), natural route 2, scores, RNG, winner/result fields, and schema-3
hashes. Expected rows may be used only for comparison, never as simulation
input.
