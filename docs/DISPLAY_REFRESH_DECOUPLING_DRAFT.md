# Display-refresh decoupling draft

Status: **sample-and-hold direction accepted; implementation parcel pending;
interpolation explicitly deferred**

Draft date: 2026-08-15

## Recommendation

Keep simulation, input, replay, PMD/audio, and source-frame publication on the
exact PC-9801 rational cadence:

```text
526315 / 9328 Hz = 56.423134648... Hz
```

For non-interpolated output, use **sample and hold**:

1. Compose and submit a complete 640x400 image only when a source owner
   publishes one.
2. Keep interval-one VSync mandatory.
3. Let the swapchain, desktop compositor, and display rescan the resident image
   until the next source publication.
4. If the display is slower than the source clock, simulation/audio/replay keep
   every source tick while host presentation keeps only the newest complete
   image.

This is already the ordinary visible production behavior: the application
submits changed source publications and the display holds the resident image.
The remaining parcel is to freeze that behavior in cross-refresh tests and
isolate simulation from a blocked present when the window is minimized or
otherwise genuinely unpresentable. It is not a new per-refresh repeat loop.

Repeatedly calling blocking
`SDL_RenderPresent()` with the same texture would create no visual information,
reduce input-poll opportunities, and can block across the next source deadline.
The physical display repeats the committed image without another app call.

## Clock ownership

| Domain | Authority | May affect deterministic play? |
|---|---|---|
| Source simulation/input/replay/audio | exact `TickInputScheduler` rational deadlines | yes; therefore frozen |
| Source presentation | complete 640x400 `SourceSurface` publications | visual output of deterministic state |
| Host submit | newest complete pending source surface | no |
| Physical scanout/VRR | SDL swapchain, DWM, driver, monitor | no |
| Future interpolation | separate display owner/mailbox | never |

Input remains timestamped and is latched only at source deadlines. Display
refresh never samples input, emits a source tick, advances PMD, consumes a
replay sample, changes RNG, or adds playtime.

## Expected repetition

| Display | Average scanouts per source image | Repeated scanouts per second |
|---:|---:|---:|
| 60 Hz | 1.06339 | 3.57687 |
| 120 Hz | 2.12679 | 63.57687 |
| 144 Hz | 2.55214 | 87.57687 |

Those are physical repeats, not dropped gameplay frames. Higher refresh can
reduce the scanout portion of input-to-photon latency but creates no extra
motion sample until interpolation exists.

At 50 Hz, some 56.4 Hz source publications cannot reach separate scanouts. The
tested contract remains: run every source/audio tick and expose the newest
complete image. Never slow gameplay to the display rate.

## Production-loop contract

```text
cutoff = monotonic_now() before event pumping
pump timestamped input
for every source deadline <= cutoff:
    latch input through that deadline
    advance runner and audio exactly once
    retain newest complete changed surface
    classify source-owned replay slowdown
if a new surface is pending and the window is presentable:
    upload and VSync-submit it once
wait/poll toward the next source deadline
```

Shipping may coalesce intermediate changed surfaces in an overdue batch while
preserving every simulation/audio tick. Pixel-oracle mode must continue to
commit each changed source boundary separately.

## Presentability and blocking isolation

The first implementation parcel should add an explicit platform
`presentable` state. While a window is genuinely minimized or occluded:

- do not call a potentially blocking host present;
- continue source simulation, replay, input, and audio policy;
- retain only the newest complete pending surface; and
- submit it once immediately after restore/expose.

Developer oracle/capture mode overrides suppression because its contract is to
retain source publications even with a hidden native window.

Focus loss is not minimization and remains non-pausing. It timestamp-clears held
input while retaining device presence; `Focus Mute` affects only final PCM
delivery. An explicit OS suspend/resume event may rebase the live scheduler and
clear held input/audio so a laptop does not execute hours of catch-up. Do not
infer suspend from a large elapsed-time heuristic.

Host-owned replay reconstruction and save/modal transitions retain the current
explicit rebase contract: no wall time spent reconstructing or in a modal is
repaid as gameplay ticks.

## Window, fullscreen, and VRR policy

- Keep one canonical 640x400 source texture.
- Keep windowed and borderless-desktop fullscreen.
- Keep interval-one VSync; do not expose `VSync Off`.
- Do not expose an FPS or refresh-rate selector. Gameplay rate is fixed, and
  borderless mode inherits the Windows desktop refresh.
- Allow driver/system VRR without a setting or guarantee. Never derive the
  source clock from a reported display mode or measured present return.
- Keep `Filtering` defined as spatial scaling; it is not temporal
  interpolation.

## Replay, slowdown, and pixel-oracle invariants

- V13 samples, checkpoints, RNG, score, and verification are independent of
  physical refresh.
- A replay at 60, 120, or 144 Hz consumes identical source samples and reaches
  identical canonical results.
- `slow_frames` remains host-performance telemetry at source completion
  boundaries. More efficient presentation may improve it; it is not
  server-authoritative gameplay state.
- T3PXH2/T3RGB1/T3ALI1 continue to record source publications/boundaries, not
  repeated physical scanouts.
- Scaling, aspect, filtering, VSync phase, DWM, and VRR stay outside the exact
  640x400 source-pixel oracle.
- Classic exposes no setting. It uses the same internal sample-and-hold and
  presentability isolation because that matches the emulator output model: a
  complete emulated PC-98 frame is delivered to the host and the display
  rescans it. This changes no Classic source state, cadence, or 640x400 image.

## Player-facing setting

Add no `FPS`, `Refresh Rate`, `VSync`, or `Frame Presentation` row now. There is
only one correct non-interpolated policy, so a visible row would be inert and
would waste scarce menu space.

When real interpolation exists, add one New Classic-only persisted enum:

```text
Frame Output        Repeat | Smooth
```

- `Repeat`: exact complete source images only; default and parity reference.
- `Smooth`: explicitly non-authentic temporal presentation; never affects
  source input, simulation, audio, replay, or verifier state.

Classic never reads or exposes it. Existing settings migrate to `Repeat`.

## Future interpolation boundary

Interpolation needs a dedicated display owner with a latest-source-frame
mailbox and display-time timestamps. It must not be implemented by changing the
simulation scheduler or by crossfading complete RGB frames, which would ghost
sprites, text, palette changes, hard cuts, flashes, and pause screens.

Before `Smooth` exists, specify separately:

- which gameplay objects and scrolling state can be interpolated;
- which menus, text, cutscenes, palettes, flashes, and hard cuts must step;
- SDL main-thread versus presentation-thread ownership;
- latency and stale-mailbox policy;
- pause, fast-forward, focus, and minimize behavior; and
- a visual suite intentionally separate from the exact source-pixel oracle.

Recommended initial scope is gameplay sprites and source scrolling only. Menus,
text, hard cuts, palettes, pause, and flashes remain stepped until each has an
explicit rule.

## No-regret implementation order

### D0 — freeze the contract

- Add platform-neutral tests proving 60/120/144 display schedules yield
  identical source inputs/state/results.
- Freeze physical repeats as zero runner/audio/replay/oracle work.
- Correct the stale `PC98_CADENCE.md` claim that production discards missed
  deadlines; production now deliberately preserves them.

### D1 — isolate blocked presentation

- Add minimized/occluded presentability at the SDL boundary.
- Retain only the newest complete pending surface while not presentable.
- Preserve oracle mode's one-source-publication-per-present behavior.
- Add test-only counts for source ticks/publications, coalesced images, and
  skipped-not-presentable submits; no release debug overlay.

### D2 — real-display validation

- Run windowed/borderless checks at 50/60/120/144 Hz and with VRR on/off.
- Exercise Alt-Tab, minimize/restore, display moves, and refresh changes.
- Re-run V13, pause/slowdown, audio, Classic, and exact source-pixel gates.

### I0 — interpolation design

Only after D0-D2 are stable, specify and build the separate `Smooth` owner.

## Remaining decisions to iterate with the product owner

1. Confirm explicit OS suspend/rebase while focus loss and minimize continue
   source simulation.
2. Confirm no VRR control or refresh selector unless field reports demand a
   troubleshooting option.
3. Reserve `Frame Output: Repeat / Smooth` as the future label without showing
   it yet.
4. Confirm gameplay/scroll-only first interpolation scope and stepped menus,
   text, palettes, pause, and flashes.
