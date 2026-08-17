# New Classic direct netplay testing

Status: cross-platform technical preview. This route runs a complete human
VS MAIN match over direct IPv4 UDP. It is intentionally exposed by command
line before the source-styled room/selection UI, WebRTC/TURN transport,
negotiated pause, replay capture, and post-match report are integrated.

Gameplay music and sound effects are enabled. Audio advances only from
contiguous-confirmed source frames, so a rollback correction cannot duplicate
or leak a speculative sound. On a low-latency tailnet this normally adds only
the actual peer-confirmation delay; the deterministic game and PMD clocks do
not follow either machine's wall clock.

The host owns logical P1 and the joiner owns logical P2. Both machines use
their ordinary local-player controls: `T/F/H/B` for directions, `R/Y/V/N` for
diagonals, `Z` for Shot, and `X` for Bomb. Escape leaves the preview. The two
commands must use the same characters, rank, seed, tick rate, executable
revision, and authenticated game data.

## Windows host from the standalone

Find the Windows Tailscale address and choose a UDP port:

```powershell
tailscale ip -4
.\PoDD-Port-Test.exe --netplay-host 31415 `
  --p1 reimu --p2 mima --rank 1 --seed 34
```

Allow `PoDD-Port-Test.exe` through Windows Firewall for the Tailscale/private
network if Windows prompts. The one-file launcher supplies its embedded
authenticated `--data`, `--main`, and `--font` inputs to the native app.

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
  --netplay-join WINDOWS_TAILSCALE_IP:31415 \
  --p1 reimu --p2 mima --rank 1 --seed 34
```

Tailscale ACLs and the host firewall must permit the selected UDP port. No
router port-forward is required inside one tailnet. The join command accepts a
literal IPv4 address in this preview, not a MagicDNS hostname.

## Current diagnostics and failure policy

The handshake rejects a different deterministic contract. Before either
source scheduler starts, both peers also hash and exchange the canonical
frame-zero `T3RBK001` simulation state. A content, seed, state, or build
disagreement therefore fails with `initial-simulation-mismatch` instead of
beginning a divergent match. A successful connection prints
`identity-frame=0`. During play, both peers exchange and acknowledge another
canonical proof every 120 contiguous-confirmed gameplay frames. Matching
proofs print `status=identity-confirmed`; a mismatch ends the preview before a
result can be accepted. Once connected,
each input datagram carries the newest 16 local values, so loss, duplication,
and reordering do not require a reliable gameplay stream. A peer times out
after five seconds without a valid packet. Prediction stops at 12 frames; MAIN
result/transition regions that are not rollback-safe switch to exact-input
lockstep and reopen rollback at the next valid boundary. Both peers print the
same confirmed boundary frames and `status=match-complete` on a clean finish.
The latest periodic proof must be mutually acknowledged before either peer may
report a clean finish or bounded smoke completion.

For a bounded automated check, append the same `--smoke-ticks N` to both
commands. This exits only after both peers have confirmed `N` gameplay input
frames. Do not change `--tick-rate` for human play.

This direct route is deliberately not the final security/privacy model. Direct
UDP exposes each tailnet IP to the other peer and supplies neither encrypted
Internet traversal nor identity. The product design in
`NEW_CLASSIC_NETPLAY_DESIGN.md` retains private rooms, authenticated signaling,
WebRTC/DTLS, TURN fallback, and a source-styled in-game flow as later work.
