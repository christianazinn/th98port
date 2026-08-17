# New Classic direct netplay testing

Status: cross-platform technical preview. This route runs a complete human
VS MAIN match over direct IPv4 UDP. It is intentionally exposed by command
line before the source-styled room/selection UI, WebRTC/TURN transport,
and post-match report are integrated. A
confirmed terminal match now continues through the ordinary winner-specific
MAINL scene, including WIN music, dismissal, and the final black-out.

Gameplay music and sound effects are enabled. Audio advances only from
contiguous-confirmed source frames, so a rollback correction cannot duplicate
or leak a speculative sound. On a low-latency tailnet this normally adds only
the actual peer-confirmation delay; the deterministic game and PMD clocks do
not follow either machine's wall clock.

The host owns logical P1 and the joiner owns logical P2. Both machines use
their ordinary local-player controls: `T/F/H/B` for directions, `R/Y/V/N` for
diagonals, `Z` for Shot, and `X` for Bomb. Escape requests a synchronized
Pause. Closing the window (including Alt+F4) leaves the preview and sends an
immediate departure to the other peer.
The host supplies P1, rank, seed, and source cadence. The joiner supplies P2.
The pre-MAIN lobby exchange combines those independently owned values and both
sides then prove the complete initial simulation. Both machines must use the
same executable revision and authenticated game data; they no longer duplicate
each other's character or room-rule arguments.

### Pause and Resume

Press Esc on either machine during active gameplay. The request is corrected
like player input, so both machines freeze only after that exact frame and its
simulation proof are confirmed. The terminal prints `status=paused`.

To resume, each player must release Esc and then press it again. The terminal
prints `status=resume-ready-local` on each machine and resumes after both
choices arrive. The wait does not advance the source clock, replay playtime,
PMD, or slowdown. The frozen gameplay frame is retained in this technical
preview; a source-styled network-pause overlay is a later UI parcel.

## Windows host from the standalone

Find the Windows Tailscale address and choose a UDP port:

```powershell
tailscale ip -4
.\PoDD-Port-Test.exe --netplay-host 31415 `
  --p1 reimu --rank 1 --seed 34
```

Allow `PoDD-Port-Test.exe` through Windows Firewall for the Tailscale/private
network if Windows prompts. The one-file launcher supplies its embedded
authenticated `--data`, `--main`, and `--font` inputs to the native app.
This authorization belongs to the manual two-machine test. Automated
verification must not launch a product listener (and must never click or
pre-authorize the firewall prompt); it uses the bounded loopback unit owner.

## Arch Linux joiner

Use a Release SDL build. With SDL 3.4.10 installed as an Arch package, obtain
the package's actual license path with `pacman -Ql sdl3 | grep LICENSE`, then:

```bash
cmake -S th03-port -B th03-port/out/build/linux-sdl -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTH03_ENABLE_SDL=ON \
  -DTH03_SDL3_PROVIDER=package \
  -DTH03_SDL3_LICENSE_FILE=/usr/share/licenses/sdl3/LICENSE.txt
cmake --build th03-port/out/build/linux-sdl -j"$(nproc)"
```

If the local SDL package uses a different license filename, substitute the
path printed by `pacman -Ql`. The already-authenticated SDL source provider is
also valid when the laptop has the repository's `out/deps/SDL3-3.4.10` tree.

Point `--data` at the same extracted TH03 process-data directory used by a
normal native run (the canonical ignored workspace layout calls it
`originals/th03_process_root`). `--main` is the corresponding original
`MAIN.EXE`; `--font` is the 288,768-byte PC-98 `FONT.ROM`:

```bash
./th03-port/out/build/linux-sdl/th03-app \
  --vs \
  --data /path/to/originals/th03_process_root \
  --main /path/to/originals/th03_full/MAIN.EXE \
  --font /path/to/FONT.ROM \
  --netplay-join WINDOWS_TAILSCALE_IP_OR_MAGICDNS_NAME:31415 \
  --p2 mima \
  --replay-root ./REPLAY
```

Tailscale ACLs and the host firewall must permit the selected UDP port. No
router port-forward is required inside one tailnet. The join command accepts
either the literal IPv4 address or an ordinary Tailscale MagicDNS hostname. It
resolves the name once before gameplay and prints the numeric `resolved-peer`;
DNS never enters the deterministic simulation.

## Current diagnostics and failure policy

The retry-safe `T3NL001` lobby refuses ownership changes and does not construct
MAIN until the host's P1/rules lock and the joiner's P2 lock form one exact
`T3NLC001` contract. Before either source scheduler starts, both peers also
hash and exchange the canonical
frame-zero `T3RBK001` simulation state. A content, seed, state, or build
disagreement therefore fails with `initial-simulation-mismatch` instead of
beginning a divergent match. A successful connection prints
`identity-frame=0`. During play, both peers exchange and acknowledge another
canonical proof every 120 contiguous-confirmed gameplay frames and at every
phase, round, and terminal boundary. Matching proofs print
`status=identity-confirmed`; a mismatch ends the preview before a result can be
accepted, and neither peer enters the next lifecycle owner before the boundary
proof is acknowledged. Once connected,
each input datagram carries the newest 16 local values, so loss, duplication,
and reordering do not require a reliable gameplay stream. A peer times out
after five seconds without a valid packet. Prediction stops at 12 frames; MAIN
result/transition regions that are not rollback-safe switch to exact-input
lockstep and reopen rollback at the next valid boundary. Both peers print the
same confirmed boundary frames, enter `status=post-match`, and print
`status=match-complete` only after their local winner scene has completed.
The latest periodic or boundary proof must be mutually acknowledged before
either peer may report a clean finish or bounded smoke completion.
On clean completion, each peer also prints local observation counters for
resolved/predicted executions, rollback requests and rewinds, resimulated
frames, prediction stalls, reliable waits, maximum zero-based prediction
depth, and missed source deadlines. These values never enter simulation, peer
agreement, or V13 replay data. Preserve both terminals' output when reporting
a test so asymmetric transport behavior remains visible.
An intentional local exit prints `status=left`; the other peer prints
`status=peer-left` rather than waiting for the crash/connectivity timeout.
The preview feeds its corrected contiguous-confirmed input stream directly into
the ordinary memory-only V13 recorder. Predicted or superseded frames never
enter it, and each machine retains its own first-execution slowdown evidence.
After the winner scene completes, a normal match prints `replay-ready=1`,
`replay-frames`, and the validated `replay-bytes` size, then opens the ordinary
authenticated New Classic Save Replay prompt/name/slot transaction. The replay
remains entirely in memory until a numbered slot is confirmed. The standalone
passes its selected persistent `REPLAY` directory automatically; a native
command should pass `--replay-root <directory>` (or defaults to `REPLAY` under
`--data`). Leaving, disconnecting, closing, or failing before the completed
winner owner writes nothing. The optional `--netplay-replay-output` developer
argument bypasses the menu and publishes the validated final V13 directly,
using the same flush/read-back/reparse/atomic-replace transaction as numbered
slots.

For a bounded automated check, append the same `--smoke-ticks N` to both
commands. This exits only after both peers have confirmed `N` gameplay input
frames. Do not change `--tick-rate` for human play.

This direct route is deliberately not the final security/privacy model. Direct
UDP exposes each tailnet IP to the other peer and supplies neither encrypted
Internet traversal nor identity. The product design in
`NEW_CLASSIC_NETPLAY_DESIGN.md` retains private rooms, authenticated signaling,
WebRTC/DTLS, TURN fallback, and a source-styled in-game flow as later work.
