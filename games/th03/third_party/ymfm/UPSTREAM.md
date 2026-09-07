# ymfm YM2608 source lock

- Upstream: <https://github.com/aaronsgiles/ymfm>
- Commit: `81aec25ccbb98f4873a255f7551ac4dadac59b4a`
- Commit date: 2026-07-27
- License: BSD-3-Clause; see `LICENSE` in this directory.

The retained `src/` files were byte-identical to the named commit at import and
provide the YM2608 OPN, SSG, ADPCM-A, and ADPCM-B implementation. Two narrowly
documented compatibility patches are now carried locally:

- `ymfm_fm.ipp` consumes each ordinary register-driven key edge immediately,
  after refreshing the operator cache. T98-Next/FMGEN does this in
  `KeyControl`; delaying both PMD key-off/key-on writes until the next generated
  sample otherwise merges the pair and causes authored FM notes to decay away.
- `ymfm_ssg.cpp` returns bipolar PSG output rather than the upstream
  zero-biased lane. This matches T98-Next/FMGEN's signed square wave and removes
  the large DC offset from the TH03 SSG mix.

The port adapter still owns clock policy, timer delivery, register tracing,
output combination, and later resampling outside this directory. Rebase audits
must reapply and reverify the two compatibility patches against the exact
upstream commit named above.
