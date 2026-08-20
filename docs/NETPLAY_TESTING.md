# New Classic netplay testing

Status: cross-platform technical preview. The source-styled `Online Match`
menu now defaults to casual Quick Match and also supports private six-character
rooms over authenticated WebSocket signaling and WebRTC/DTLS data channels.
Direct IPv4 UDP remains an explicit Network Settings fallback for
LAN/Tailscale testing. A
confirmed terminal match now continues through the ordinary winner-specific
MAINL scene, including WIN music, dismissal, and the final black-out.

Gameplay music and sound effects are enabled. Audio advances only from
contiguous-confirmed source frames, so a rollback correction cannot duplicate
or leak a speculative sound. On a low-latency tailnet this normally adds only
the actual peer-confirmation delay; the deterministic game and PMD clocks do
not follow either machine's wall clock.

The host owns logical P1 and the joiner owns logical P2, but each machine has
only one local keyboard. Both therefore use the ordinary default Story
controls: arrow keys for movement, `Z` for Shot, `X` for Bomb, and Left Shift
for Charge only when the local Story Autofire option is enabled. Each peer's
Story Autofire choice is frozen into the match contract and projected onto
that peer's negotiated P1 or P2 lane. The special local-VS `T/F/H/B` and
numpad columns are not active. Escape requests a
synchronized Pause. Closing the window (including Alt+F4) leaves the preview
and sends an immediate departure to the other peer.
The host supplies P1, rank, seed, source cadence, and the common 0..6-frame
gameplay input delay. The joiner supplies P2.
Each product host launch generates a fresh nonzero match seed. If both players
select the same character and palette, P1 keeps the selected palette and the
negotiated contract moves P2 to the opposite palette.
The pre-MAIN lobby exchange combines those independently owned values and both
sides then prove the complete initial simulation. Both machines must use the
same executable revision and authenticated game data; they no longer duplicate
each other's character or room-rule arguments.

## Socket-free automated acceptance

`th03-netplay-two-process-tests` launches separate host and join native worker
processes and relays only inherited stdin/stdout bytes. The workers link the
production platform-neutral netplay library but no UDP or RTC adapter, so this
test cannot bind a port or trigger a firewall prompt. It covers the exact lobby,
input, periodic proof/ack, Pause/Resume, Forfeit/ack, terminal proof/ack, and
disconnect wires. Its accepted transcript SHA-256 is
`2C1CCBCC8BFA2F106E16B53A6AD54FAD258758FD1D711CBBCD260AD11C443E5C`.

This is a local process/protocol gate, not evidence of ICE or TURN behavior.
Real Windows/Linux direct and relayed acceptance remains a private-beta gate.

## Room-code setup

The reviewed room service is deployed with the existing site Worker at
`https://azinn.dev`. Current product builds use that origin automatically.
`TH03_ONLINE_ORIGIN` remains an explicit staging/developer override; setting
it to an empty value disables online-service access:

```text
TH03_ONLINE_ORIGIN=https://staging-worker.example
```

On Windows, the first confirmed room launch automatically enrolls one opaque
service identity and stores it as current-user DPAPI data in `PODD.AUTH` beside
`PODD.CFG`. Later launches reuse that identity. It never enters the settings,
replay, room code, deterministic state, or logs. Deleting `PODD.AUTH` is the
low-level manual reset operation. The product UI exposes the same bounded
operation at `Option -> System -> Online & Privacy -> Forget Room Identity`,
behind an explicit confirmation. It never displays the user ID or token.

On Arch/Linux, the first confirmed room launch enrolls the same opaque service
identity over the authenticated WebSocket stack and atomically stores
`PODD.AUTH` with owner-only `0600` permissions beside `PODD.CFG`. The token is
never shown or logged. This is a deliberately small private-beta credential
owner until an approved desktop secret-store abstraction exists. One public IP
is not an identity: NAT can place several players behind it, addresses change,
and IPv6 addresses are privacy-sensitive.

The following environment flow remains available as a developer recovery and
automation override:

```bash
umask 077
mkdir -p ~/.config/podd-port
curl -fsS -X POST https://azinn.dev/api/th03/users | jq -r \
  '"export TH03_ONLINE_USER_ID=" + (.userId|@sh) + "\n" +
   "export TH03_ONLINE_TOKEN=" + (.token|@sh)' \
  > ~/.config/podd-port/online.env
source ~/.config/podd-port/online.env
unset TH03_ONLINE_ORIGIN
```

Current builds default to `https://azinn.dev`; an
`online-origin-missing` diagnostic specifically means
`TH03_ONLINE_ORIGIN` is present but empty, so remove it with `unset` rather
than exporting an empty value. Never share the token or commit `online.env`.

Setting both user/token variables on Windows remains an explicit developer
override. Supplying only one, or malformed values, fails closed.

`TH03_NETPLAY_ICE_SERVERS` is an optional developer-only semicolon-delimited
list of unauthenticated `stun:`, `turn:`, or `turns:` URLs. It can remain unset
for a same-tailnet test where host ICE candidates are mutually reachable. A
configured room service supplies bounded short-lived TURN URLs, username, and
credential in its authenticated `T3ROOM2` welcome; these values remain outside
replays, deterministic state, settings, and diagnostics.

`Network Settings -> Connection` selects `Automatic` or `Relay Only` while the
Room Code Service is active. Automatic attempts direct peer-to-peer ICE and
then the supplied relay. Relay Only prevents direct candidates and fails
closed if the service did not issue credentialed TURN. It may add latency but
does not reveal a direct peer address. The Direct UDP fallback replaces this
row with the host-port editor and does not use TURN.
Connection, route policy, and the Direct UDP host port persist in `PODD.CFG`;
Quick Match always uses the online service without changing a saved Direct UDP
preference.
The Direct UDP host port is edited as a five-digit numeric field and accepts
only the unprivileged range 1024..65535. The host waiting surface displays that
port and tells the joining player to combine it with the host's LAN or
Tailscale address. Direct Join uses SDL's physical text input, so keyboard
layout and Shift produce punctuation such as `:` normally; letters, including
`X`, are text rather than gameplay Bomb/Backspace while the editor owns focus.
`Input Delay (frames)` is also persisted. The host's selected value is shown to
both peers and frozen into the ready contract. It delays only active rollback
gameplay; menus, interstitials, synchronized Pause, and terminal scenes retain
their exact source input timing. Automatic RTT-based tuning remains deferred
until private-beta measurements exist.

Open `VS Start` -> `Online Match` on both machines. For casual pairing, both
select `Quick Match`, choose their local character/palette and the same rank,
then wait; the service pairs two distinct users and randomly assigns sides.
For a private match, the creator selects `Create Room`; the assigned code
appears on its waiting screen. The other player selects `Join Room` and types
that code. Character/palette locks and rank are still exchanged and verified
by `T3NL001` before MAIN is constructed.
Create Room never asks the host to invent a code: its confirmation states that
the service assigns one. If the service origin, identity, signaling endpoint,
ICE path, or peer connection is unavailable, the product displays a
recoverable connection report. Cancel from that report returns to the exact
Quick Match, Create Room, or Join Room caller; Cancel from a waiting lobby does
the same. Other nested Cancel actions unwind one Online Match screen, and only
Cancel from the Online Match root returns to the New Classic title. The
already-running OP.M session advances throughout these retained screens and
continues without a Stop/Load/Play restart when the title is revealed again.
No direct listening socket is opened by this path, avoiding the Windows
Firewall listener prompt. The same rollback, Pause, replay, and report owners
below apply to both transports.

After both character locks are accepted, the matched surface immediately
stops OP.M and plays the short `WIN.M` round-clear cue. The character-versus
MAINL screen must not appear until the cue reaches its natural end; there is no
fixed five-second countdown. Both peers must keep responding to Cancel and
network traffic during the cue. Cancel or a connection failure before gameplay
restores title music and returns to the calling room screen.

The tracked SDL presets enable the RTC adapter. Before configuring a source
build, authenticate SDL as documented elsewhere, clone libdatachannel v0.24.3
recursively at commit `c6696d157b5612df2a741d9a03b192b47ab6cefb`, acquire
the official Mbed TLS 3.6.7 archive, and run:

```powershell
pwsh th03-port/tools/bootstrap_rtc.ps1 `
  -LibDataChannelSource C:\path\to\libdatachannel `
  -MbedTlsArchive C:\path\to\mbedtls-3.6.7.tar.bz2
```

The bootstrap refuses unpinned/dirty source, validates every recursive
submodule pin and the Mbed TLS SHA-256, and stages all required licenses.

### Pause and Resume

Press Esc on either machine during active gameplay. The request is corrected
like player input, so both machines freeze only after that exact frame and its
simulation proof are confirmed. The terminal prints `status=paused`.

To resume, each player must release Esc and then press it again. The terminal
prints `status=resume-ready-local` on each machine. The first ready peer keeps
the authenticated Pause modal visible with cyan `Waiting for Opponent` text.
Once both choices arrive, both clients replace it with the same visible cyan
`Resume in 3`, `2`, `1` countdown and
resume only after its full three wall-clock seconds. The wait and countdown do
not advance the source clock, replay playtime,
PMD, or slowdown. Both machines retain the same confirmed gameplay frame and
draw the authenticated PAUSED artwork over it. Up/Down selects between Resume
and a source-styled `Forfeit` row; Z/Return confirms and Q forfeits directly.
A confirmed Forfeit ends
both previews with complementary `forfeit-outcome=loss`/`win` results (or
`mutual` for crossed requests), writes no replay, and never invents a TH03
winner. Restart and Save remain unavailable while the match is live.

## Direct UDP fallback: Windows host from the standalone

Find the Windows Tailscale address and choose a UDP port:

```powershell
tailscale ip -4
.\PoDD-Port-Test.exe --netplay-host 31415 `
  --p1 reimu --rank 1 --seed 34
```

The host window appears immediately and waits without a pre-peer deadline, so
it is safe to start the laptop afterward. Closing the window before a peer
joins is a clean cancellation. Once a peer participates, an incomplete lobby
exchange still fails after 30 seconds as `peer-handshake-timeout`; a joiner
pointed at the wrong address has the same bounded deadline.

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
  -DTH03_ENABLE_RTC=ON \
  -DTH03_SDL3_PROVIDER=package \
  -DTH03_SDL3_LICENSE_FILE=/usr/share/licenses/sdl3/LICENSE.txt \
  -DTH03_RTC_PROVIDER=source \
  -DTH03_LIBDATACHANNEL_SOURCE_DIR="$PWD/th03-port/out/deps/libdatachannel-0.24.3" \
  -DTH03_MBEDTLS_SOURCE_DIR="$PWD/th03-port/out/deps/mbedtls-3.6.7"
cmake --build th03-port/out/build/linux-sdl -j"$(nproc)"
```

If the local SDL package uses a different license filename, substitute the
path printed by `pacman -Ql`. The already-authenticated SDL source provider is
also valid when the laptop has the repository's `out/deps/SDL3-3.4.10` tree.
The maintained Mbed TLS WebSocket build verifies `wss://` signaling against
the POSIX system CA bundle. It honors `SSL_CERT_FILE` when set, then checks the
standard Arch/Debian/Fedora/macOS bundle locations; a missing or invalid bundle
fails closed as `tls-ca-unavailable` rather than disabling verification.

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
  --netplay-input-delay 2 \
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
depth, missed source deadlines, and—when WebRTC supplies one—the latest
bounded round-trip sample in milliseconds. The authenticated report displays
the same sample as `Round Trip (ms)`; direct UDP displays `--` because it has
no equivalent truthful measurement. These values never enter simulation,
peer agreement, or V13 replay data. Preserve both terminals' output when
reporting a test so asymmetric transport behavior remains visible.
Delayed or duplicated UDP runtime-proof packets and acknowledgements older
than the newest verified proof are ignored; current and future identities
remain strict. A `peer-timeout` immediately after the other process reports a
proof failure is a cascading symptom, not a second deterministic divergence.
An intentional local exit prints `status=left`; the other peer prints
`status=peer-left` rather than waiting for the crash/connectivity timeout.
Forfeit is different from closing the application: it is bound to the exact
confirmed Pause identity, retries a typed request/ack, and both sides print
`status=match-forfeited` plus `replay-ready=0` and `replay-published=0`.
The preview feeds its corrected contiguous-confirmed input stream directly into
the ordinary memory-only V13 recorder. Predicted or superseded frames never
enter it, and each machine retains its own first-execution slowdown evidence.
After the winner scene completes, a normal match prints `replay-ready=1`,
`replay-frames`, and the validated `replay-bytes` size, then opens the ordinary
authenticated New Classic Save Replay prompt/name/slot transaction. The replay
remains entirely in memory until a numbered slot is confirmed. The standalone
passes its selected persistent `REPLAY` directory automatically; a native
command should pass `--replay-root <directory>` (or defaults to `REPLAY` under
`--data`). Leaving, forfeiting, disconnecting, closing, or failing before the completed
winner owner writes nothing. The optional `--netplay-replay-output` developer
argument bypasses the menu and publishes the validated final V13 directly,
using the same flush/read-back/reparse/atomic-replace transaction as numbered
slots.

V13 remains the portable replay artifact and keeps canonical P1/P2 character,
score, winner, and input ordering. A numbered netplay save also writes a
48-byte installation-local `TH3RNN.PODD` view beside `TH3RNN.RPY`. Its
`PODDRIX1` record stores only whether this installation was P1 or P2 and binds
that projection to the exact replay SHA-256. The Replay list and save summary
therefore show the local peer's character and final score, and the detail page
labels the match `Vs Mode Netplay`. Missing, malformed, copied without the
replay, or digest-stale sidecars are ignored; the valid V13 falls back to the
ordinary local-VS presentation. V13 already identifies the canonical player
roles, but does not contain online user names or account identities. Adding
those is a future replay-format decision rather than a reserved-byte shortcut.

For a bounded automated check, append the same `--smoke-ticks N` to both
commands. This exits only after both peers have confirmed `N` gameplay input
frames. Do not change `--tick-rate` for human play.

This direct route is deliberately not the final security/privacy model. Direct
UDP exposes each tailnet IP to the other peer and supplies neither encrypted
Internet traversal nor identity. The product design in
`NEW_CLASSIC_NETPLAY_DESIGN.md` retains private rooms, authenticated signaling,
WebRTC/DTLS, TURN fallback, and a source-styled in-game flow as later work.
