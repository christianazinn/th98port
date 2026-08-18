# New Classic netplay design

Status: **local rollback, the direct-UDP/Tailscale fallback, the product room
frontend, private rooms, casual Quick Match, and the pinned native WebRTC
transport are implemented. The local service and native adapter own bounded
short-lived TURN credentials and Automatic/Relay Only policy; production
Cloudflare configuration and real cross-machine Quick Match acceptance remain
deployment gates.**

Proposal revision date: 2026-08-17

This document specifies portable rollback netplay for New Classic's human
1P-vs-2P mode. It deliberately does not add networking, configuration, UI, or
different timing to Classic. Classic remains the sealed local port of the base
game.

## Proposed decision summary

- Add `Online Match` as the fourth New Classic-only row under `VS Start`. This
  reuses the row occupied by `Quit` in the Classic four-row VS submenu, so the
  source-sized window does not need to grow. Cancel still returns to the title.
- Keep private room codes and add a deliberately simple, rank-separated casual
  Quick Match queue. Spectators, live reconnect, ratings, and ranked online VS
  boards remain later products.
- Use peer-to-peer rollback during active `GM_VS_1P_2P` MAIN gameplay. Menus,
  character selection, result screens, and replay saving are synchronized
  control phases, not predicted rollback state.
- Prefer GGRS as the rollback scheduler behind a versioned C ABI. It is not
  allowed to own rendering, audio, game state, or product policy. An initial
  build/performance spike must pass objective gates before the dependency is
  frozen. The same interface permits an app-owned C++ backend if that spike
  fails.
- Use libdatachannel WebRTC Data Channels behind an app-owned transport
  interface. Gameplay uses a discrete unordered, unreliable channel; control
  uses a separate reliable, ordered channel. ICE tries direct peer-to-peer
  UDP, with TURN relay fallback.
- Extend the existing Cloudflare service with one ephemeral Durable Object per
  room for authentication, room codes, player assignment, SDP/ICE signaling,
  and short-lived TURN credentials. Gameplay packets never traverse the
  Worker or Durable Object.
- Preserve the exact `526315/9328` Hz source simulation cadence. Rollback
  predicts inputs, never time. Host catch-up may execute several complete
  source ticks without presenting intermediate frames, but it may not change
  a tick's rules or duration.
- Record only the final resolved input history into the existing in-memory V13
  recorder. Predicted and superseded executions never enter the replay.

The architecture is intentionally layered. GGRS, libdatachannel, Cloudflare,
and the product UI can each be replaced without changing deterministic TH03
gameplay or the rollback-state contract.

## Product contract

### Entry and room flow

The New Classic title retains its existing rows. `VS Start` becomes:

1. 1P vs CPU
2. 1P vs 2P
3. CPU vs CPU
4. Online Match

`Online Match` opens a source-styled screen with:

- Quick Match
- Create Room
- Join Room
- Network Settings
- Return

Create Room displays a short, non-secret room code. Join Room uses the existing
name-entry visual language for that code. The existing opaque online user ID
and token authenticate room creation and joining; enrollment can remain
automatic and does not become a public account system.

Quick Match asks only for the local character, palette, and rank. One
authenticated hibernating Durable Object per rank pairs the oldest two
different users and randomly assigns P1/P2. It then reuses private-room
`T3ROOM2` signaling, TURN, immutable match contract, rollback, replay, and
report owners. There is no rating, region selection, persistent ticket,
gameplay relay, or server-declared winner.

After connection, both peers see the same P1-left/P2-right selection layout.
The room creator is P1 in private rooms. Quick Match assigns sides with a
server coin flip. Each local cursor responds immediately; reliable control
messages replicate cursor and lock state. Character selection itself is not
rolled back. Once both players lock, both clients construct the same immutable
match contract and enter a ready barrier before MAIN starts.

### Supported first release

- New Classic `game_mode::vs_1p_2p` only.
- Two human peers, one locally owned player per peer.
- Quick Match or private room-code connection.
- Direct ICE path with TURN fallback and an optional Relay Only privacy mode.
- Source-styled character selection, stage splash, active match, result, and
  return flow.
- Automatic in-memory V13 capture of the resolved match.
- A negotiated network pause, forfeit, disconnect handling, desync detection,
  and a local post-match network report.
- Windows, Linux, and macOS as protocol/build targets. A platform that does not
  yet ship the port must not be designed out of the protocol.

### Deliberately deferred

- Classic netplay.
- 1P-vs-CPU or CPU-vs-CPU online modes.
- Ratings, tournaments, or an online VS leaderboard.
- Spectators and mid-match join.
- State-transfer reconnect or migration to a new peer.
- More than two players.
- Voice, text chat, friend lists, invitations, and presence beyond one room.
- An authoritative gameplay server or server-hosted simulation.
- Anti-cheat claims. A modified client can still automate input or lie about
  host presentation; mutual deterministic agreement is not proof of human
  play.

### Match-affecting and local settings

Only settings that change deterministic gameplay enter the match contract.

| Setting | Ownership | Contract |
|---|---|---|
| Rank | Room | Selected by the creator and displayed before lock |
| Character and palette | Per player | Frozen at character lock |
| Autofire | Per player | Combined into the agreed P1/P2 mask and shown to both |
| Initial RNG seed | Room service | Random server-issued 32-bit value |
| Three-digit combos and New Classic rules | Ruleset | Fixed by `RulesetId`, not configurable per room |
| Input delay | Network | Agreed before each round; displayed, never hidden |
| Maximum prediction | Protocol | Fixed at 12 frames for v1 |
| Key/controller mapping | Local | Applied before the logical input boundary |
| Music/SFX/master volume and mute-unfocused | Local | Never serialized or hashed as gameplay |
| Language, window, fullscreen, scaling | Local | Never serialized or hashed as gameplay |
| Reduced Flash FX | Local presentation | May differ without producing a gameplay desync |

Online MAIN always uses one canonical `key_mode` value in deterministic state.
Device assignment stays outside the core, so a keyboard, joystick, and a
different peer's device configuration cannot produce otherwise irrelevant
state differences.

## Architecture and dependency direction

```text
Cloudflare Worker / room Durable Object
    |  reliable WebSocket signaling, room contract, TURN credentials
    v
platform RTC adapter (libdatachannel)
    |  opaque discrete gameplay packets + reliable control messages
    v
portable netplay session owner
    |  rollback requests, phase policy, replay journal, diagnostics
    v
rollback backend C ABI (preferred backend: GGRS)
    |  save / load / advance requests only
    v
MainProcess rollback adapter
    |  CoreInput at one exact source boundary
    v
th03_core + th03_present
```

The existing boundaries remain binding:

- `th03_core` owns deterministic fixed-width state and logical input. It gains
  typed state capture/restore but no socket, path, clock, SDL, WebRTC, GGRS, or
  Rust type.
- `th03_present` owns PC-98 raster/TRAM and presenter state. It gains typed
  local capture/restore where required for visual rollback, but no network
  policy.
- `th03_formats` remains the V13 byte adapter. It is not the rollback snapshot
  format.
- `th03_replay` remains a sequential input/process coordinator. Predicted
  frames are resolved before they reach it.
- A new platform-neutral `th03_app_netplay` target owns match phases, logical
  player input, rollback requests, control-message validation, confirmed-frame
  replay journaling, and diagnostics.
- A new `th03_netplay_ggrs` Rust `staticlib` exposes only a frozen C ABI and
  owns GGRS allocations. No Rust panic or C++ exception may cross that ABI.
- A new `th03_platform_rtc` target owns libdatachannel callbacks, ICE/STUN/TURN,
  signaling WebSockets, and thread-safe packet queues. Network callbacks never
  step the game directly.
- The SDL executable composes these owners. SDL remains an input, display, and
  audio endpoint rather than a network protocol owner.

The leaderboard Worker and the room signaling service may share deployment,
authentication, and operational tooling, but they are separate authority
domains. Leaderboard verification may persist accepted V13 files. A room
Durable Object stores no gameplay stream or mutable TH03 state.

## Rollback middleware decision

### Preferred backend: GGRS behind a C ABI

GGRS is preferred over directly embedding the official GGPO repository because
it already separates rollback scheduling from a custom `NonBlockingSocket`.
Its request-driven API maps naturally to the port's retained owners: the app
fulfills ordered Save, Load, and Advance requests rather than surrendering game
ownership to callbacks. Its transport trait explicitly supports discrete,
unordered, unreliable transports such as WebRTC Data Channels.

The adapter must not expose Rust or GGRS structures in C++ headers. Its public
surface consists of opaque session handles, fixed-width input values, bounded
byte spans, event codes, and ordered rollback requests. Rust owns every Rust
allocation. C++ copies a canonical rollback snapshot into a supplied Rust-owned
cell on Save and restores from a borrowed read-only cell on Load.

All exported functions use a non-unwinding C ABI and convert failure into an
explicit status. The adapter catches unwind-capable panics at its outer edge;
release builds also fail closed on a panic. C++ callbacks entering Rust are
`noexcept`, and no C++ exception crosses the boundary.

The exact GGRS source revision, Cargo dependency graph, Rust toolchain, license,
and archive digest must be pinned and vendored for offline builds. `latest` is
not an authority. Windows/MSVC, Linux/GCC, macOS/Clang x64, and macOS/Clang
arm64 must build the same C ABI and pass the same protocol vectors.
GGRS is dual MIT/Apache-2.0; the chosen license and notices must ship with the
binary. libdatachannel is MPL-2.0 and likewise requires a pinned license/source
compliance review before distribution.

### Required middleware spike

GGRS becomes the frozen production backend only if the first milestone proves:

- clean static-library integration in the supported CMake presets;
- reproducible offline builds with an audited dependency and license bundle;
- no panics, exceptions, allocator ownership, or borrowed-lifetime ambiguity
  across the C ABI;
- exact scripted behavior under loss, duplication, reordering, and jitter;
- one-frame sync-test rollback on every source frame for a long VS corpus;
- bounded snapshot memory with no source-tick heap allocation after warm-up;
- snapshot/restore plus common rollback resimulation comfortably below one
  17.723 ms source deadline on the minimum supported machine.

If it fails, `th03_app_netplay` keeps the same interface and receives a small
app-owned C++ rollback coordinator. That fallback implements only two-player
input prediction, snapshot rings, confirmed-frame advancement, time sync, and
desync checks. It does not port GGPO's bundled UDP stack. This is a fallback,
not an invitation to maintain two production backends.

The official GGPO code remains a behavioral and sync-test reference. Its
developer guide is especially useful for the complete-state, single-frame,
render-suppressed rollback requirements, but its direct address/UDP session
shape is not the product transport.

## Rollback domain and state

### Domain boundary

Rollback begins at the first input boundary of an authenticated, source-fresh
human VS MAIN process. It persists across route-1 repeat rounds in that same
process. It ends when MAIN reaches a terminal route that hands ownership back
to MAINL/OP policy.

Character selection, room UI, a terminal win screen, save UI, and network
errors are not part of rollback state. They use reliable control messages and
explicit ready barriers. This avoids attempting to serialize the complete OP
and MAINL application hierarchy while still covering every frame whose remote
input can affect gameplay.

### Why current snapshots are insufficient

Current V13 `RuntimeSnapshot`, `CompactRuntimeSnapshot`, and
`RoundStartSnapshot` values are portable round-start recipes. They do not
capture an arbitrary gameplay frame. `CanonicalState` is a compact diagnostic
view and hash, not restorable state. `Game` and `MainProcess` are intentionally
non-copyable retained owners. Netplay therefore requires a new complete,
field-wise state contract; it must not reinterpret a C++ object image.

### `T3RBK001` in-memory snapshot

`T3RBK001` is a private, versioned in-memory schema. It is never a replay file,
never accepted from a peer, and never written as a normal user artifact. A
debug build may serialize it only into a local desync diagnostic.

The snapshot has two sections:

1. **Simulation state**: every mutable value that can affect a future gameplay
   result, including `Game` state, RNG/rings/cursors, object pools and active
   counts, player/enemy/bullet state, round/result state, deferred-frame state,
   and deterministic presenter feedback scheduling inputs.
2. **Local presentation state**: the complete MAIN Raster pages, shown/access
   page, raster mode/scroll, palette and tone state, TRAM and mutable gaiji,
   private presenter pools/cursors/animations, pending semantic audio command
   queue, and other state needed to reproduce the next correct surface after a
   restore.

Immutable authenticated content, decoded static assets, font bytes, PMD song
data, SDL objects, audio-device buffers, paths, network handles, clocks,
threads, and replay storage are excluded. Snapshots contain no pointers,
padding, host-sized integers, containers with implementation-defined layouts,
or borrowed lifetimes. Capture and restore are explicit field-wise operations.

The full local snapshot is restored during rollback because the current
presenter is history-dependent. Only the simulation section is cross-peer
authoritative. Local settings such as Reduced Flash may legitimately make the
presentation section differ. A SHA-256 simulation digest is used for peer
desync checks; a separate full-snapshot digest supports local sync-test and
pixel diagnosis.

The snapshot is captured at the input boundary before rollback frame `F` and
restores exactly that boundary. A fixed-capacity ring stores at least
`max_prediction + 2` boundaries. V1 fixes maximum prediction to 12 and begins
with 16 slots. The implementation must measure actual bytes and throughput
before freezing a memory cap; the initial engineering target is no more than
32 MiB for the complete rollback ring and no allocation after match warm-up.

Resimulation runs the normal core and presenter observers with host
presentation and audio delivery suppressed. Only the final corrected surface
is offered to the display mailbox. This avoids maintaining a second renderer
or attempting to reconstruct history-dependent PC-98 state from a gameplay
hash.

## Input contract and prediction

The network input is the post-device, post-KeyConfig logical input for exactly
one owned player. It contains:

- one 16-bit TH03 player input word;
- that player's one-bit Charge/Autofire contribution;
- one typed Pause-request edge.

The C ABI value is exactly four bytes: little-endian `uint16 input_word`,
`uint8 charge`, and `uint8 control`. It has a golden wire vector and can be
copied into a rollback-backend input type without exposing C++ padding or Rust
layout.

P1 owns only `CoreInput.p1` and charge bit 0. P2 owns only `CoreInput.p2` and
charge bit 1. Peers may not inject the other player's word, arbitrary
`CoreInput.system`, application quit, replay fast-forward, restart, raw
scancodes, or host-device identity. The session combines the two owned values
into one `CoreInput` after the rollback backend resolves them; only that owner
may project a resolved Pause edge to source Cancel.

The predictor repeats the last known remote input, including held movement and
buttons. That matches TH03's level-triggered input semantics better than
predicting neutral and avoids inventing button edges. The Pause edge is always
predicted neutral. The first prediction is neutral. Real input replacing a
different prediction requests rollback.

System actions use reliable control messages:

- Pause is negotiated at a confirmed frame.
- Resume uses a ready barrier and countdown.
- Forfeit and disconnect terminate prediction.
- Host application quit is local and sends best-effort disconnect control.

No match setting changes during a round. An input-delay change may be proposed
and acknowledged only at a confirmed round boundary.

## Cadence, delay, and time synchronization

One exact source tick is:

```text
9328 / 526315 seconds = 17.723226583... ms
```

| Frames | Added input delay / rollback span |
|---:|---:|
| 0 | 0.000000 ms |
| 1 | 17.723227 ms |
| 2 | 35.446453 ms |
| 3 | 53.169680 ms |
| 4 | 70.892906 ms |
| 6 | 106.339360 ms |
| 8 | 141.785813 ms |
| 12 | 212.678719 ms |

Before character lock, peers exchange timestamped probes for at least three
seconds. `Auto` estimates one-way transit as half the p95 RTT, adds observed
jitter margin, converts the result upward to source frames, and clamps the
common delay to 1..6. Both players see the agreed delay and current ping.
Manual 0..6 selection can be added to Network Settings for testing and expert
use; hidden asymmetric delay is not allowed in v1.

The initial deterministic recommendation is:

```text
margin = max(2 ms, (RTT_p95 - RTT_median) / 2)
peer_delay = ceil((RTT_p95 / 2 + margin) / source_tick)
common_delay = clamp(max(peer_delay_p1, peer_delay_p2), 1, 6)
```

This is a starting policy, not a claim that RTT is symmetric. The private beta
must retain raw measurements and may revise the formula between protocol
releases rather than silently tuning it during a match.

Maximum prediction is 12 frames. If remote input is still unknown beyond that
window, simulation stalls on the newest complete surface instead of predicting
further or dropping source frames. Sustained stalls trigger the disconnect
policy.

The source rate never changes. Rollback middleware time-sync recommendations
map to host scheduling only:

- a peer ahead idles and repeats the latest complete display frame;
- a peer behind may execute a bounded number of complete source ticks in one
  host iteration and present only the newest result;
- PMD service, input sampling, replay timing, and gameplay still observe every
  source tick in order;
- neither peer simulates a fractional, 60 Hz, refresh-rate-derived, or
  delta-seconds gameplay frame.

Delay may increase or decrease by at most one frame between rounds after both
peers acknowledge it. It never changes invisibly during a round. The UI reports
RTT, jitter, direct/relay path, current delay, rollback frames in the last
second, and stalls.

## Transport and Cloudflare signaling

### WebRTC channels

libdatachannel is the preferred native transport because it supplies a C/C++
WebRTC implementation across Windows, Linux, and macOS and supports a small C
API. Build it without media support and pin its exact source archive, options,
license, and transitive dependencies.

Each peer connection opens two binary Data Channels:

| Channel | Delivery | Owner |
|---|---|---|
| `th03-game/1` | unordered, unreliable, zero retransmits | Opaque GGRS packets |
| `th03-control/1` | ordered, reliable | Handshake, selection, pause, result, diagnostics |

GGRS already owns gameplay-packet reliability, ordering, prediction, and
acknowledgement. The app must not layer a conflicting home-grown ACK protocol
around those bytes. The custom `NonBlockingSocket` serializes GGRS messages in
Rust and hands opaque bounded datagrams to the RTC adapter. Packets larger than
the measured no-fragmentation budget are a test failure; the initial target is
at most 1200 bytes.

The reliable control channel carries small, versioned, explicitly bounded
messages. It never carries a rollback snapshot or replay file. Large replay
uploads continue through the authenticated HTTPS leaderboard path after the
match, not through the peer connection.

WebRTC supplies DTLS confidentiality/integrity and ICE traversal. Automatic
mode first tries a direct candidate and falls back to TURN. Relay Only excludes
host/reflexive candidates so the remote peer does not receive the player's
public address through a direct path. The UI must explain that Automatic
peer-to-peer play can reveal peer IP addresses.

### Room service

Add a separate room Durable Object to the existing Cloudflare deployment. One
object owns one short-lived room and at most two authenticated sockets. It:

- creates a cryptographically random room ID and short display code;
- assigns P1/P2, issues the seed, and records the proposed match contract;
- relays bounded SDP descriptions, ICE candidates, and pre-connection room
  control;
- issues or brokers short-lived TURN credentials without exposing the TURN
  service key;
- expires waiting rooms and destroys state after closure.

It does not relay gameplay Data Channel messages, inspect logical inputs,
advance TH03, choose a winner, or persist a replay. Durable Object WebSockets
are signaling/control infrastructure, not a substitute for the low-latency
gameplay channel. Once the peer connection's reliable control Data Channel is
open, character selection, pause, result, and close messages move peer-to-peer;
the Durable Object is not their per-frame intermediary.

The implemented routes are:

```text
POST /api/th03/netplay/rooms
GET  /api/th03/netplay/rooms/new          (create + WebSocket upgrade)
GET  /api/th03/netplay/rooms/{code}       (WebSocket upgrade)
```

`T3ROOM2` includes the sanitized short-lived STUN/TURN configuration in the
authenticated `welcome` message. Issuing it at the room upgrade avoids a
second bearer-authenticated HTTP client in the native adapter and binds the
credentials to the same short-lived room lifecycle. The Worker alone calls the
provider credential API; its account token is never forwarded to the Durable
Object or either peer.

Room codes are lookup handles, not authorization secrets. The existing bearer
credential authenticates the socket. All descriptions, candidates, control
messages, counts, and text have strict limits.

## Handshake and compatibility

`T3NET002` is the current owned-input/control-edge protocol. Before character
lock, both clients must agree on:

- protocol major/minor and exact rollback backend wire revision;
- `ContentId`: canonical digest of gameplay-affecting content;
- `RulesetId`: canonical digest of New Classic gameplay policy;
- `RollbackSchemaId`: digest of the `T3RBK001` simulation schema;
- source cadence numerator/denominator;
- player slot, room ID, match ID, and server seed;
- rank, both characters/palettes, and both Autofire choices;
- maximum prediction and negotiated input delay;
- recorder/replay capability required for the match.

Cross-platform executable hashes are intentionally not required to match. A
Windows and macOS build may connect when their content, rules, rollback schema,
protocol, and deterministic preflight hashes match. A human-readable build ID
is exchanged for diagnostics only.

Handshake values use fixed-width little-endian fields or one canonical
serialization with golden vectors. Unknown mandatory fields, nonzero reserved
bits, duplicate messages, out-of-phase messages, and inconsistent repetitions
fail closed. No path, locale text, OS username, device GUID, or memory address
enters the contract.

After agreement, both clients create source-fresh MAIN state and exchange the
SHA-256 simulation digest at frame zero. Gameplay begins only if those digests
match. The direct UDP preview implements this gate with a strict fixed-size
`T3NP` v6 packet and keeps both source schedulers stopped until the proof
succeeds.

The backend-neutral owned-player gameplay value is now frozen as the exact
four-byte little-endian schema in `T3NET002.md`. It contains one player's
post-KeyConfig logical word, binary Charge contribution, and one typed Pause
edge. The match contract supplies player ownership; the retained MAIN owner is
the only code that projects the marker to source Cancel, and arbitrary
system/application input is never accepted through this stream.

## Desync handling and replay ownership

Every confirmed round start, every 120 confirmed gameplay frames, and the
terminal frame exchange a SHA-256 digest of canonical simulation-state bytes.
The rollback backend's lightweight checksum may detect errors earlier, but it
does not replace the application digest.

A mismatch immediately stops prediction at a confirmed boundary. Neither peer
accepts a winner or uploads a record. Each writes a bounded `T3NETD001` local
diagnostic containing IDs, frame numbers, recent resolved inputs, local hashes,
network metrics, and phase events. It contains no auth token, SDP, ICE address,
or remote snapshot. There is no automatic state transfer or "host wins"
repair; either would mask deterministic bugs and make one modified peer an
authority over the other.

The direct preview implements mutually acknowledged proofs at frame zero,
every 120 contiguous-confirmed gameplay frames, and every confirmed phase,
round, or terminal boundary. A periodic milestone coinciding with a lifecycle
boundary emits one typed boundary proof and still advances the periodic
schedule. Both peers remain stopped at that boundary until the exact post-frame
digest, proof kind, boundary identity, and acknowledgement agree. The bounded
proof/ack retransmission therefore cannot carry a latent mismatch into the next
source lifecycle owner or report a divergent result as clean.

Both peers maintain a frame-indexed replay journal outside rollback state.
Predicted executions may overwrite an unconfirmed journal row. Only contiguous
confirmed rows and their confirmed process/round events are committed to
`NewClassicReplayRecorder`. The recorder therefore sees one canonical sample
per frame and never observes rollback duplicates.

The journal retains the first-execution local source-deadline classification
for each resolved gameplay frame. An intentional network prediction stall,
ready barrier, or negotiated pause is not reported as machine slowdown merely
because wall-clock time passed without a source tick. Actual missed deadlines
while the local session was allowed to advance remain local slowdown evidence;
the two peers' report-only slowdown metadata may therefore differ.

V13 already accepts `GM_VS_1P_2P`, so the resolved match remains an ordinary
portable replay. No network packet or rollback snapshot is embedded. User
name/date fields may make two locally saved V13 byte streams differ, so peers
compare a separate `T3NTR001` transcript digest over the match contract,
resolved inputs, confirmed boundaries, splits, and terminal state. Matching
transcript digests prove that both peers agreed on the simulated match.

A normal terminal match enters the existing replay-save flow. Forfeit or
disconnect may expose a clearly labeled local partial replay, but it is never
eligible for public upload. Closing the application discards all unsaved
in-memory material.

## Presentation and audio during rollback

Rollback resimulation never publishes intermediate corrected frames to SDL or
the pixel oracle. The final corrected source surface replaces the display
mailbox's pending frame. Ordinary sample-and-hold presentation then applies.
Local scaling, fullscreen, language, Flash FX, and volume remain independent.

The audio device cannot be rewound. The direct-UDP preview therefore journals
each source frame's semantic command transaction beside its rollback slot,
replaces that transaction during correction, and advances PMD only when the
frame becomes contiguous-confirmed. Process-start music commands are
non-speculative, and repeat/terminal audio is held until its boundary is
confirmed. This safe first policy is implemented and tested on both peers.

For later Internet play, semantic commands generated during active MAIN can
receive a local `(match frame, command ordinal, command)` event ID:

- BGM load/play/stop and other phase-changing commands execute only after their
  control/terminal boundary is confirmed.
- A gameplay one-shot SFX plays on its first speculative execution for
  responsiveness. Resimulation suppresses the same event ID rather than
  playing it twice.
- If later input invalidates a speculative sound, it cannot be unheard. This is
  an accepted local presentation artifact, not gameplay state.
- PMD/YM2608 synthesis itself is never part of the cross-peer rollback hash.

That responsive one-shot policy is preferable if measured Internet confirmation
delay makes effects feel late. It remains future work and requires focused
duplicate-suppression tests; it must not allow an audio queue to mutate
deterministic gameplay. The current confirmed-only preview never waits an
arbitrary twelve frames when the peer has already supplied the input.

## Pause, disconnect, and lifecycle

Esc during online gameplay sends a Pause Request instead of opening the local
Replay Patch pause menu immediately. At the next mutually confirmed frame,
both peers stop prediction and display authenticated PAUSED artwork over the
retained confirmed frame. Save Replay and Restart are unavailable while the
match is live. The source `Discard and Exit` row is the visible Forfeit action:
it exchanges a typed request/ack bound to that Pause identity, discards the
incomplete memory-only replay, and exits without forging a gameplay winner.

Resume requires both peers to become ready, then starts after a short shared
countdown. Focus loss never silently pauses a peer; the existing local
mute-unfocused option still applies. The pause is represented once in the
confirmed replay journal as a canonical source-compatible Pause -> Resume
transaction, not as every wall-clock poll of the network waiting screen. A V13
playback can therefore preserve the gameplay boundary without needing to know
room state or reproduce an arbitrary Internet wait.

Temporary packet loss stalls after the 12-frame prediction limit. A lost peer
gets a bounded grace period before the match becomes disconnected. V1 does not
accept a new connection or remote state transfer after that point. A player may
forfeit cleanly at any time. Application quit sends a best-effort close event
but does not block shutdown on the network.

Exact pause timeout and disconnect grace values remain tuning constants until
the induced-loss beta; their values are not part of gameplay rules and must be
shown rather than silently deciding a winner.

The direct preview now ends normal completion, either side of Forfeit, peer
departure, peer inactivity, and deterministic proof mismatch on one
authenticated `SLB1.PI` + `MNUFONT.PF` report. It shows bounded local prediction,
rollback, resimulation, wait, and deadline counters only. Those observations
are deliberately outside simulation, proof, and V13. The report requires an
interface-word release followed by a fresh Z/Return/Esc; application close
remains immediate.

## Security and privacy boundaries

- DTLS protects Data Channel traffic in transit, but a peer is still an
  untrusted endpoint.
- Every message is bounded and phase-checked before allocation or mutation.
- Remote snapshots, filesystem paths, content bytes, locale strings, and audio
  payloads are never accepted.
- The Worker rate-limits room creation and TURN credential issuance by the
  authenticated opaque user ID. It stores only salted token hashes.
- TURN service credentials are short-lived and generated server-side.
- Logs redact bearer tokens, SDP, ICE candidates, and IP addresses by default.
- Direct P2P may reveal network addresses to the peer. Relay Only is the
  privacy alternative and may add latency.
- Deterministic agreement proves only that two clients produced the same
  transcript under the declared contract. It does not prove an unmodified
  executable, human input, or real-time play.

No result from netplay enters the existing Story leaderboard service until a
separate VS board and verifier policy are designed. Reusing authentication
does not silently broaden leaderboard acceptance.

## Objective validation gates

### State and determinism

- Capture -> mutate -> restore reproduces every field and the exact next
  simulation/presentation output.
- One-frame rollback on every frame for long source-fresh VS sessions produces
  the same simulation digest, surfaces, terminal transition, and V13 transcript
  as a no-rollback run.
- Forced rollback across every frame phase, pause boundary, route-1 repeat, and
  terminal boundary has no stale state.
- GCC, MSVC, and Clang produce identical simulation digests and transcript IDs
  for the frozen cases.
- Canonical snapshot encoding has golden vectors and rejects padding,
  truncation, unknown versions, and nonzero reserved fields.

### Network simulation

- Deterministic tests cover 0..300 ms latency, asymmetric latency, 0..10% loss,
  burst loss, duplication, reordering, jitter, and temporary partition.
- Two-process loopback and cross-host tests reproduce the same transcript.
- Prediction stalls at 12 frames and never skips or invents a source tick.
- Delay changes occur only at acknowledged round boundaries.
- Control messages are idempotent and rejected outside their legal phase.

### Presentation, audio, and replay

- No speculative intermediate surface is presented during rollback.
- Corrected final pixels equal a no-rollback execution from the same resolved
  inputs, including both Raster pages and TRAM/gaiji state.
- SFX event IDs prevent duplicates; confirmed BGM transitions occur once.
- Both peers produce the same `T3NTR001` digest and independently verified V13
  gameplay for completed matches.
- Predicted inputs and discarded branches never appear in V13.
- A desync, forfeit, or disconnect cannot be uploaded as a completed result.

### Transport and platform

- Windows/MSVC, Linux/GCC, macOS/Clang x64, and macOS/Clang arm64 pass the same
  C ABI and packet/control golden vectors.
- Direct LAN, ordinary NAT, IPv6, TURN/UDP, TURN/TCP, and TURN/TLS paths are
  exercised. Relay Only emits no direct candidate to the remote peer.
- Shutdown, callback races, channel loss, and malformed packets pass sanitizers
  where available and never call game code from a network thread.
- Dependency sources, toolchains, build flags, licenses, and hashes are pinned;
  release packaging contains no debug protocol diagnostics by default.

## Implementation order

### N0: rollback-state proof, no networking

1. Enumerate complete mutable `Game`, MAIN driver, presenter, Raster, TRAM, and
   feedback state. The `Game` simulation owner, shared Raster/TRAM visual
   machines, private retained MAIN presenters, and process semantic-audio state
   are complete.
2. Add field-wise `T3RBK001` capture/restore and simulation/full digests. The
   architecture-neutral simulation image and shared visual image are now
   implemented, transactionally restored, and measured. The fixed-capacity
   presenter state is field-wise and measured, and `T3RBM001` composes the
   complete local process image transactionally. Peer SHA-256 and the local
   frame-ring coordinator remain open. See `T3RBK001.md`, `T3RBV001.md`,
   `T3RBP001.md`, and `T3RBM001.md`.
3. Add a local rollback owner and one-frame-every-frame sync test. Implemented
   as the synchronous `T3RBC001` proof: 48 consecutive corrected frames match
   uninterrupted state, pixels, and audio while private predicted output is
   inaccessible.
4. Measure snapshot bytes, capture/restore time, resimulation time, and memory.
   The sixteen-slot ring plus recovery is 10,794,988 bytes on GCC x64. The
   conservative optimized wrong/restore/correct/restore probe measures about
   2.75 ms per corrected frame on the development host. See `T3RBC001.md`.

This highest-risk local proof is green. Canonical peer simulation SHA-256,
confirmed-frame audio publication, and mutually acknowledged 120-gameplay-
frame runtime proofs are now integrated. Minimum-machine measurements remain
an explicit N1 gate rather than a reason to alter the rollback image.

### N1: middleware and simulated transport

1. Freeze the backend-neutral owned-player input value. Complete as
   `T3NET001`: exact four-byte wire, strict validation, and independent P1/P2
   composition. `T3NET002` assigns its reserved byte to a rollback-correct
   Pause edge without expanding the value.
2. Add a deterministic in-process latency/loss/reorder simulator and resolved
   input timeline around the existing local coordinator. The backend-neutral
   transport/timeline half is complete as `T3NTS001`: two peers converge
   through asymmetric delay, primary loss, duplication, reordering,
   prediction, and rewind. Its retained MAIN integration is complete as
   `T3NMS001`: delayed corrections restore and resimulate the complete process,
   suppress intermediate output, and keep speculative boundaries private.
   Confirmed-frame audio journaling is also complete: corrected commands
   replace speculative suffixes and both peers advance identical PMD VSyncs.
3. Build the pinned GGRS C ABI spike and custom queued `NonBlockingSocket`.
4. Fulfill Save/Load/Advance requests from `MainProcess`.
5. Freeze prediction, delay, confirmation, digest, and replay-journal behavior.
   Prediction, delay, confirmation, confirmed audio, frame-zero identity, and
   mutually acknowledged 120-frame gameplay digests are frozen for the direct
   preview. The corrected journal now feeds the existing memory-only V13
   recorder with source waits, round/process controls, and local slowdown
   evidence. Explicit round/terminal network digests remain open.
6. Decide GGRS versus the single fallback backend using the objective spike
   gates; delete the rejected production path.

### N2: portable peer transport and local signaling

The immediately testable direct-IPv4 UDP/Tailscale preview is complete on the
app-owned transport: fixed handshake/history packets, bounded prediction,
frame-zero and 120-frame simulation identity, full MAIN repeat/terminal
lifecycle, confirmed audio, prompt intentional departure, and Windows/POSIX
socket tests. A crash or connectivity loss still uses the bounded inactivity
timeout. The room transport is now implemented as follows:

The product transport now pins libdatachannel 0.24.3 and Mbed TLS 3.6.7 behind
an app-owned adapter. `T3NP07-gameplay` is unordered/unreliable with no
retransmits; `T3NC01-control` is reliable and ordered. The maintained SDL
presets build this adapter, while direct UDP remains a selectable fallback.

Still open:

1. Add a hermetic two-process loopback acceptance fixture that does not create
   an unexpected public firewall listener.
2. Prove Windows/Linux/macOS packet and transcript interoperability over real
   direct ICE and relayed paths.

The completed local report now carries the latest bounded WebRTC round-trip
sample alongside rollback, prediction, wait, and missed-deadline counters.
The value is sampled at a low host-side cadence and remains advisory: it never
enters deterministic state, proof hashes, replay data, or peer authority.
Direct UDP reports the value as unavailable because that transport does not
yet expose a truthful RTT measurement.

### N3: product integration

1. Add the fourth VS submenu row, room UI, character-lock control, and ready
   barrier. The backend-neutral 32-byte `T3NLC001` match value now freezes
   host-owned P1/rank/seed/cadence, join-owned P2, and New Classic ruleset 1;
   the exact 64-byte `T3NL001` owner now exchanges those independent locks and
   gates MAIN over both room service and direct UDP fallback. The fourth row,
   source-styled room/code editor, synchronized lock, waiting, and ready
   presentation are implemented without changing Classic.
2. Add negotiated pause, forfeit, disconnect, desync UI, and post-match report.
   The direct preview now carries a rollback-correct Pause edge, confirmed
   boundary proof, release-before-Resume input, two-peer ready barrier, and
   authenticated PAUSED/Resume presentation. The synchronized Forfeit policy
   and request/ack lifecycle are now complete in the direct preview. Completed
   matches, Forfeit, peer departure/timeout, and deterministic proof mismatch
   now enter the authenticated local report. Product-room ownership of that
   terminal is owned by the fourth-row frontend.
3. Feed only confirmed history into the in-memory V13 recorder and existing save
   UI. Complete in the direct preview: the corrected contiguous-confirmed
   transcript becomes an ordinary validated V13 only after the complete VS
   winner MAINL owner, then enters the authenticated numbered Save Replay
   transaction. No working file exists during play and incomplete matches
   publish nothing.
4. Keep every Classic construction and hash unchanged.

### N4: Cloudflare private-room beta

1. Add the room Durable Object and signaling routes beside the existing online
   service without deploying until reviewed. The local service now has a
   fail-closed `T3ROOM2` two-seat, ten-minute Durable Object plus authenticated
   create/connect routes and the native RTC/WebSocket adapter. Production
   bindings remain intentionally absent.
2. Short-lived TURN credential issuance and operational redaction are
   implemented locally. Automatic tries direct ICE before relay; Relay Only
   fails closed unless the authenticated welcome contains credentialed TURN.
   Real provider resources and secrets remain deployment inputs.
3. Measure real direct and relayed RTT/jitter/rollback distributions. The
   product report now exposes bounded WebRTC RTT; jitter and distribution
   collection remain external private-beta evidence rather than game state.
4. Run a bounded private beta and tune delay/stall/disconnect policy.

### Later product decisions

- Quick Match region/rating policy.
- Spectator architecture and replay-delayed spectating.
- Whether any VS results belong on an online board.
- Reconnect through explicit trusted state transfer, if its complexity and
  authority tradeoff become worthwhile.
- Whether a future replay format should embed a netplay transcript ID. V13 does
  not need to change for the first release.

## Rejected approaches

- **Directly vendor official GGPO as the product stack.** Its useful reference
  API is coupled to a direct address/UDP session and the upstream build is
  Windows-oriented. Porting that transport would still leave NAT traversal,
  relay, signaling, and non-Windows maintenance to this project.
- **Delay-only lockstep.** It is simpler but converts network transit and jitter
  directly into visible input delay or stalls. It remains a diagnostic mode
  through a zero prediction window, not the default product.
- **Worker/Durable Object WebSockets for gameplay.** They are reliable TCP
  signaling connections. Head-of-line blocking and a mandatory central hop are
  wrong for per-frame prediction traffic.
- **Authoritative server simulation.** It adds permanent compute cost and an
  extra network leg, while deterministic two-player simulation already exists.
  It would still not prove human input.
- **Permanent custom rollback before a middleware spike.** Snapshot ownership
  is project-specific; prediction, time sync, packet reliability, and edge
  behavior are not. Reimplement them only if the maintained backend fails a
  measured integration gate.
- **Raw `memcpy` of C++ owners.** Object images contain pointers, padding,
  allocator state, and compiler-specific layout. They are neither portable nor
  complete contracts.
- **Use V13 checkpoints as rollback saves.** They are round-start recipes, not
  arbitrary-frame process images.
- **Rollback the complete OP/MAINL application.** Remote gameplay input cannot
  affect most of those scenes; reliable phase barriers are smaller and safer.
- **Send state to repair desyncs.** That hides deterministic defects and makes
  one untrusted client authoritative.
- **Run at 60 Hz or the monitor refresh rate.** Netplay changes where inputs
  become known, not TH03's source cadence.
- **Add netplay to Classic.** Classic remains the exact local experience.

## Primary references and current source anchors

- The official [GGPO Developer Guide](https://github.com/pond3r/ggpo/blob/master/doc/DeveloperGuide.md)
  requires deterministic, serializable game state and single-frame execution
  without rendering, and documents its save/load/sync-test model.
- Current [GGRS documentation](https://docs.rs/ggrs/latest/ggrs/) describes its
  request-driven Save/Load/Advance flow; its
  [`NonBlockingSocket`](https://docs.rs/ggrs/latest/ggrs/trait.NonBlockingSocket.html)
  explicitly accepts WebRTC-like discrete unordered unreliable transports.
- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) documents
  Windows, Linux, and macOS support, C bindings, ICE/STUN/TURN, and WebRTC Data
  Channels. Its
  [C API](https://github.com/paullouisageneau/libdatachannel/blob/master/DOC.md)
  exposes ordered/unordered and reliable/unreliable channel configuration.
- [RFC 8831](https://www.rfc-editor.org/rfc/rfc8831) specifies WebRTC Data
  Channels over SCTP/DTLS and their ordered/unordered, reliable/partially
  reliable delivery modes.
- Cloudflare documents
  [Durable Object WebSockets](https://developers.cloudflare.com/durable-objects/best-practices/websockets/)
  for room coordination and
  [Realtime TURN](https://developers.cloudflare.com/realtime/turn/) plus
  [short-lived credentials](https://developers.cloudflare.com/realtime/turn/generate-credentials/)
  for relay fallback.
- Rust's official
  [FFI guidance](https://doc.rust-lang.org/nomicon/ffi.html) requires explicit
  unwind discipline across language boundaries.
- `include/th03/core/game.hpp` defines fixed-width `CoreInput`,
  `GM_VS_1P_2P`, round-start recipes, diagnostic `CanonicalState`, and the
  non-copyable `Game` owner.
- `include/th03/app/main_process.hpp` exposes one guarded source-fresh human VS
  `MainProcess` and no arbitrary-frame save/load API.
- `docs/REPLAY_VERIFICATION.md` documents why V13 checkpoints are portable
  recipes rather than native process images.
- `docs/PLAYABLE_PORT_ROADMAP.md` freezes core/present/platform dependency
  direction and the exact `526315/9328` Hz input scheduler.
- `src/formats/v13.cpp` and `src/app/new_classic_replay.cpp` already accept and
  record `game_mode::vs_1p_2p`.
- `docs/NEW_CLASSIC_ONLINE_SERVICE_DRAFT.md` owns the existing user credential,
  Cloudflare deployment, V13 upload, and leaderboard trust boundaries that the
  room service must not silently broaden.
