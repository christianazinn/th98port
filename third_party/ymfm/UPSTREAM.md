# ymfm YM2608 source lock

- Upstream: <https://github.com/aaronsgiles/ymfm>
- Commit: `81aec25ccbb98f4873a255f7551ac4dadac59b4a`
- Commit date: 2026-07-27
- License: BSD-3-Clause; see `LICENSE` in this directory.

The retained `src/` files are the unmodified sources needed by the YM2608 OPN,
SSG, ADPCM-A, and ADPCM-B implementation. Every retained file was byte-identical
to the named commit at import. The port adapter, clock policy, timer delivery,
register tracing, output combination, and later resampling remain outside this
directory.
