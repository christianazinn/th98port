# Phase 0 oracle contract

The frozen corpus contains 551,191 logical samples and 3,901 canonical states
per lineage. All 18 PC-98 runs reached their natural terminal and all 12
Master-to-Anniversary / Master-to-Replay comparisons report `same`.

| case | V11 SHA-256 | T3CASE SHA-256 | samples | canonical rows |
|---|---|---|---:|---:|
| TH3R00 | `4675bd3d4c2cd55d5402843923f39fe78d5994548e6d6ce5f146496d17057246` | `626b77d4f00802e7ee065faf813351e5344e62d677004913f534fe9ea330cb7c` | 100642 | 715 |
| TH3R01 | `3be1059a75815702b0bca0f9f02eda1697c8308f3ae1fcb20dbe6a0e27f8dac9` | `b5347245a27929f590dcaa39e72ce09f3e3c6f6a4ff0d2a456876df9b7f47ba3` | 135215 | 964 |
| TH3R02 | `e378d1e5807c984d9e7cd139a27a4d1c4a054484cda4b5f835297e1a90b6d2b1` | `15a906f8680b9b324456e627fccd6c90939d834e7acf4132021b93348ca1d8c4` | 91939 | 626 |
| TH3R03 | `d07232f2559f8b9015fdf2942cdffcf0518abdc78e43b1b638e83dd9d277b77e` | `62b28fc4910abac56cf5677ad56bf8306292b8a4027775a85703cdb0b0485ef8` | 25211 | 186 |
| TH3R04 | `468ca6d2a33fc3f14b088a179451296d5b16efa197249286279b0d7999089070` | `98e516ecc1a8dac38514ed959fd5410373af01c2952142084aa87fb05e1b0c6f` | 86437 | 627 |
| TH3R05 | `e674542eb9515358cb17c0afbe7a2c8f701bd959c0d2ea8f5e7e85a75fdcf38a` | `4a007c121694a81c6bead0404029dfda791813ee06a0a461fb0cbe676bdfc6d5` | 111747 | 783 |

`th03-replay-check` reproduces the calibrated binary split comparison:

- duplicate Replay `start`/`round_start` pairs canonicalize once;
- checkpoint global-frame labels may differ by a consistent -1, 0, or +1;
- round-start and checkpoint hashes are hard gates;
- route/finish hash differences remain visible diagnostics while explicit
  terminal gameplay fields stay hard gates; and
- shorter output is a failure unless candidate-prefix mode is explicit.

TH3R03 intentionally reports two terminal hash diagnostics. PC-98 probes proved
that only a post-route input latch differs; score, winner, route, player/chain
state, counters, and live RNG agree. No native comparator exception hides this.

Run `tools/verify_phase0_external.ps1` to validate the ignored evidence in
place. Never copy the corpus, normalized cases, split captures, executables, or
original assets into this repository.
