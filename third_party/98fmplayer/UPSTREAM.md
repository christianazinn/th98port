# 98fmplayer PMD sequencer source lock

- Upstream: <https://github.com/Myon98/98fmplayer>
- Commit: `4fa914e4b2b994cb3ccf92d571a20a7cdf1fe36a`
- Commit date: 2024-07-23
- License: BSD-2-Clause; see `LICENSE` in this directory.

The port retains only the PMD sequencer's six source/header files. At import,
`fmdriver_common.c`, `fmdriver_common.h`, `fmdriver_pmd.c`,
`fmdriver_pmd.h`, and `pmd_ssgeff.h` were byte-identical to the named commit.

`fmdriver.h` has one extraction patch, marked in that file: the upstream
`ppz8.h` include is replaced by its nine-entry callback declaration. This
severs an unused PPZ8/`stdatomic.h`/visualizer dependency that MSVC cannot parse
as C. The port always binds the guarded PPZ8 pointers to null because TH03 ships
no PPZ8 data. No other upstream file is modified.

The driver accepts caller-owned mutable PMD byte images and emits chip access
through callbacks. It owns no filesystem or audio device. Its external FM
effect implementation is incomplete; the port does not treat this import as an
implementation of TH03's `.EFC` service.
