# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. Phase 2 now implements and oracle-verifies one
complete TH3R03 round on top of the Phase 1 format/session runway. There is
intentionally no playable application, renderer, audio target, networking, or
CI configuration.

## Build and test

Requirements are CMake 3.25 or newer and a C++23 compiler. There are no
third-party dependencies.

```powershell
cmake --preset msvc-x64
cmake --build --preset msvc-x64
ctest --preset msvc-x64

cmake --preset gcc
cmake --build --preset gcc
ctest --preset gcc
```

The MSVC preset uses Visual Studio 2022 x64. The GCC preset uses the standard
`MinGW Makefiles` generator.

Validate an external frozen case without copying it into this repository:

```powershell
out\build\msvc-x64\Debug\th03-replay-check.exe --case X:\path\TH3R03.T3C
out\build\msvc-x64\Debug\th03-headless.exe --case X:\path\TH3R03.T3C
```

Pass the calibrated ENEDAT and frozen Anniversary split to execute the Phase 2
gate:

```powershell
out\build\msvc-x64\Debug\th03-headless.exe `
  --case X:\path\TH3R03.T3C `
  --enedat X:\path\ENEDAT.DAT `
  --oracle X:\path\T3SPLIT.BIN
```

The command must report `oracle_status=same-prefix`, 46/186 rows, a natural
terminal after 6,773 samples, route 2, and final scores `01090503`/`00434365`.
The remaining oracle rows belong to later MAIN processes and are Phase 3 work.

Run the complete external Phase 0 lock with:

```powershell
tools\verify_phase0_external.ps1 -EvidenceRoot X:\path\to\rec98-harness
```

The evidence root must contain the ignored six-file V11 corpus, normalized
T3CASE cases, and final split artifacts. The script checks frozen SHA-256 and
sample counts, parses all twelve replay documents, and reruns all twelve native
split comparisons. Original game data and replay captures remain external.

## Boundaries

- `th03_core` owns deterministic state and stepping. It has no filesystem,
  platform, rendering, audio, or network dependency.
- `th03_formats` owns byte-level formats and adapts them to a neutral
  `ReplaySession`. It operates on byte spans and performs no file I/O.
- `th03-headless` and `th03-replay-check` are independent filesystem/CLI
  clients of those libraries.
- V11 is validation-only. V12 is not implemented. The finalized V13 reader will
  produce the same neutral session type, while a separate tool converts V11 and
  V12 into V13.

See `docs/ARCHITECTURE.md`, `docs/ORACLE_CONTRACT.md`, and `docs/SOURCES.lock`
before expanding Phase 3 coverage.
