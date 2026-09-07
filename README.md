# Touhou PC-98 Native

Native ports and replay verification for the five PC-98 Touhou games.
This repository continues the complete history of `libth03`.

`games/th03/` contains the existing PoDD port, deterministic gameplay core,
replay verifier, video renderer, and application. `games/th01/` contains the
early Highly Responsive to Prayers foundations. TH02, TH04, and TH05 native
gameplay implementations have not started. A successful build of this repository
does not claim that those games can run or verify replays.

Each game owns its simulation, replay adapters, presentation, and application
targets. Gameplay is independent of host clocks, file I/O, graphics, audio,
and networking. Common code is extracted only when tested consumers establish
the same contract. There is no shared game-state abstraction.

## Build

The default build needs CMake 3.25 and a C++23 compiler. It builds both existing
games and runs their tests without original assets or SDL dependencies:

```sh
cmake -S . -B out/build/native -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/native --config Release --parallel
ctest --test-dir out/build/native -C Release --output-on-failure
```

Windows also has `gcc` (MinGW) and `msvc-x64` presets. Use
`-DPC98_BUILD_TH01=OFF` or `-DPC98_BUILD_TH03=OFF` to select a game.
Per-game builds remain available, for example `cmake -S games/th03 -B out/podd`.

The maintained PoDD SDL presets, standalone packager, and renderer GUI builder
run from `games/th03/`; see [the PoDD documentation](games/th03/README.md).
The manual beta workflow remains at the repository root and packages PoDD.
No original game assets, replay corpus, or private release tooling is included.

The TH03 verifier container is built from the repository root with
`docker build -f Dockerfile.verifier .`, so its build context includes `shared/`.
Its separately supplied private resource remains under
`games/th03/verifier-private/`, excluded from Git.

## Moving an existing checkout

Fetch and check out this branch normally. Existing ignored `out/` contents
stay where they are. CMake build caches contain absolute source paths and
must be regenerated in a fresh build directory after this source move.
Downloaded dependency sources can be reused by passing their absolute paths
through the existing `TH03_SDL3_SOURCE_DIR`,
`TH03_LIBDATACHANNEL_SOURCE_DIR`, and `TH03_MBEDTLS_SOURCE_DIR` options.

The planned upstream name is `christianazinn/touhou-pc98-native`. Until the
owner renames it, the remote remains `christianazinn/libth03`; both refer to
this same native-only history. In the private harness it is one submodule at
`native-port/`. The ReC98 source and private harness are separate repositories.
