# Application ownership tree

Status: active behavior-free layout contract.

`src/app/` and `include/th03/app/` mirror application ownership. They do not
form new runtime layers and they do not replace the target boundaries in
[ARCHITECTURE.md](ARCHITECTURE.md). Their purpose is to make ownership visible
without requiring a global filename search through a flat directory.

| Directory | Owner |
|---|---|
| `classic/` | Shared Classic orchestration and rendering |
| `classic/mainl/` | MAINL gameplay executable behavior |
| `classic/op/` | OP title and menu executable behavior |
| `classic/persistence/` | Classic score/config persistence |
| `classic/scenes/` | Classic scene transitions and interstitials |
| `content/` | Shared app-side resource loading |
| `direct_vs/` | Direct VS host/session entry |
| `main/` | Native MAIN process and scene composition |
| `new_classic/` | Shared New Classic orchestration |
| `new_classic/content/` | Localization, fonts, and picture/text patches |
| `new_classic/input/` | Bindings, devices, and input presentation |
| `new_classic/netplay/` | Deterministic online session and transport glue |
| `new_classic/online/` | Profiles, leaderboards, and online replay services |
| `new_classic/persistence/` | New Classic settings and replay persistence |
| `new_classic/replay/` | Replay browsing, rendering, and video export |
| `oracle/` | Pixel-oracle application identity |
| `runtime/` | Platform-neutral app loop/runtime boundary |
| `update/` | Update manifest, installer, and release configuration |

## Placement rules

- No source or public header belongs directly in either application root. The
  configure-time CMake guard rejects one.
- Public includes name the complete owner path, for example
  `th03/app/new_classic/netplay/new_classic_netplay_main_session.hpp`.
- Private implementation headers are addressed from `src/app`, for example
  `new_classic/content/new_classic_language_internal.hpp`.
- A physical move does not justify a namespace, target, dependency, or behavior
  change. Those require their own reviewed parcel.
- Prefer an existing owner over inventing a one-file directory. Add a new owner
  only when its lifecycle or dependency boundary is distinct.

`th03_app_main` still contains several scene owners despite this clearer
physical tree. Any later target-level decomposition must follow an observed
dependency graph and remain separate from this behavior-free move.

Within `classic/op/`, `classic_op_session.cpp` owns only the public retained
state machine. The concrete resource, persistence, replay, and MAIN/MAINL
backend lives in `classic_op_session_backend.cpp` and is returned through a
source-private factory bundle. Scene advancement and host-facing requests use
separate operation tables; the state machine must never cast their opaque
contexts back to the production backend type.

Within `new_classic/`, the local replay browser remains in
`new_classic_frontend.cpp`, while the replay-save editor and its art preparation
live in `replay/new_classic_replay_save_menu.cpp`. Common replay-list formatting
is confined to the source-private `new_classic_replay_presentation` helper;
replay-save state must not leak back into the browser owner. The neutral mapping
from editor commands to menu input lives independently in
`new_classic_text_editor.cpp` so profile and network text fields do not depend
on either replay UI.

## Executable composition

`apps/th03-app/main.cpp` owns composition: selecting product modes, connecting
platform adapters, and coordinating process-lifetime flows. Cohesive private
implementations that are not reusable library APIs belong under
`apps/th03-app/support/`; for example, authenticated optional rhythm-profile
loading lives in `support/rhythm_assets.*`, while usage and machine-readable
error reporting live in `support/diagnostics.*`. Process-environment access
and the startup policy derived from it live in `support/runtime_environment.*`.
The platform-specific UDP/RTC channel adapter lives in `support/netplay_wire.*`;
replay-save resource preparation, modal driving, retained audio, catalog
refresh, and slot publication live together in `support/replay_save.*`; the
complete direct-netplay lobby and match lifecycle lives in
`support/netplay_session.*`, with nonce/lock and UDP/RTC pre-match negotiation
separated internally in `support/netplay_lobby.cpp`. Their source-private
contract carries only the negotiated wire, lobby identity, match contract, and
paired profiles. The composition root creates the platform and product-shell
requests but does not own transport negotiation, deterministic advancement,
rollback, pause barriers, reports, or netplay replay capture.

Platform selection for product-online HTTP and credential persistence belongs
to `support/product_online.*`. Callers consume one executable-private adapter
and do not select Windows or POSIX transports in the composition root.

The retained Online Match room/profile screens are driven by
`support/online_frontend.*`. That owner contains SDL text-input capture, modal
tick/presentation loops, title-audio advancement, and cancellable background
request waits. The composition root consumes its result values and owns only
product navigation and settings decisions.

Host key labels and controller enumeration callbacks live in
`support/input_catalog.*`. The product shell binds these callbacks into New
Classic, while the SDL vocabulary and mapper projection remain platform-input
details.

Process-lifetime update activation and trial-health state lives in
`support/update_host.*`. Startup initializes it once, launch paths acknowledge
health through the host, and sessions query the current runtime context without
reaching into composition-root globals.

Once a Classic or New Classic run has been launched,
`support/experience_session.*` owns the runner, audio device, persistence,
online/update workers, replay completion, and paced presentation loop until it
returns a lifecycle disposition. The composition root owns profile transitions
and reacts to that result; it does not advance a running game.

Support modules must expose the smallest interface needed by the composition
root. Moving a concern there does not make it a public library facility, and
must not change its validation, diagnostics, or runtime behavior. A facility
needed by another executable should instead receive an explicit library owner
in a separate dependency-design change.
