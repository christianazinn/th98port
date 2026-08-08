# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. The current Phase 1 delivers the executable
validation runway: exact numeric primitives, strict T3CASE/V11/T3SPLIT readers,
a canonical split writer/comparator, neutral replay sessions, and headless
command paths. Gameplay simulation begins in Phase 2; there is intentionally no
playable application, renderer, audio target, networking, or CI configuration.

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

The second command validates the complete case, constructs a neutral replay
session and `Game`, then exits with code 3 and
`result=simulation-not-implemented`. That result is the deliberate Phase 1/2
boundary, not a parser failure.

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
before Phase 2 work.
