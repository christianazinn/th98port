# PC-98 cadence contract

The Classic and direct-VS application paths default to the nominal PC-9801
24 kHz display cadence:

```text
21,052,600 / (8 * 106 * 440) = 526315 / 9328 Hz
```

This is an exact rational for the nominal timing model. It is not a claim that
every physical oscillator has zero tolerance. `--tick-rate` remains available
as a developer/host override, and the rate-neutral scheduler retains exact
rational deadlines without floating point.

## Evidence

- Pinned ReC98 Anniversary commit
  `3e2381d4275be7287f5525de5c95389d492b0a3c`,
  `libs/master.lib/vsync.asm`: the 24 kHz path sets `vsync_Delay` to zero, so
  every raw vertical interrupt advances the game counters. The 31 kHz path is
  distinct: it adds 13,311 to a 16-bit accumulator and suppresses the counter
  increment on carry, yielding a counted/raw ratio of `52225/65536`.
- DOSBox-X release 2026.05.02 commit
  `d35990b71de99dd837252c9353eb14f96ccfa275`: the PC-98 24 kHz path uses a
  21,052,600 Hz oscillator divided by 8. Its default master-GDC SYNC bytes
  decode to 80 active plus 26 blanking character periods and 400 active plus
  40 blanking lines.
- NP2kai commit `e2dc9046aa5c786fcfbfb87e883457e421026e31`
  independently uses the same oscillator, divider, SYNC bytes, and derived
  totals.
- Iiyama's ProLite B2409HDS/E2209HDS timing table lists PC9801 640x400 at
  21.053 MHz, 24.827 kHz, and 56.424 Hz. Sony's VPL-CX10/CS10 table lists
  24.823 kHz and 56.416 Hz. This small published spread is why the application
  constant is named *nominal* rather than *physical-exact*.

## Phase and scheduling policy

Anniversary MAIN enters its first process frame without a pre-wait. The host
therefore makes sample zero immediately due when the scheduler is created, then
uses cumulative-ceiling rational deadlines for later samples. Deterministic
smoke/replay callers preserve every overdue deadline. Interactive callers
follow source-style `frame_delay(1)` behavior by rebasing missed retraces rather
than simulating a growing catch-up backlog.

Host VSync never defines the logical rate. It only paces publication of the
newest completed 640x400 source surface.

## Remaining empirical gates

A guest PIT capture can confirm a particular emulator configuration and a
physical trace can measure a particular oscillator. Neither changes the
nominal default. Page/palette/pixel captures, 31 kHz behavioral comparison, and
character-select stale-counter artifacts remain separate visual-oracle work.
