# Port research notes

These notes define implementation boundaries for work after the deterministic
gameplay core. They are research inputs, not commitments to a particular UI or
third-party dependency:

- [`PRESENTATION_CONTRACT.md`](PRESENTATION_CONTRACT.md) inventories the
  source presentation schedule, freezes the gameplay-only optional observation
  boundary, and defines presenter-owned raster and visual-oracle gates.
- [`ASSET_INVENTORY.md`](ASSET_INVENTORY.md) inventories the extracted TH03
  data without copying original bytes and proposes the `th03_assets` decoder
  boundary. Its first RGB/PI decoder parcel is now implemented with synthetic
  tracked tests; original files remain external validation inputs.
- [`AUDIO_PMD_YM2608.md`](AUDIO_PMD_YM2608.md) inventories PMD/YM2608 behavior,
  evaluates implementation sources and licenses, and defines an audio proof of
  concept and validation artifacts.

Original game data, reference binaries, and generated PCM/frame captures stay
outside the standalone source repository. The initial `th03_assets` decoder
and `th03_present` indexed-raster foundations remain stdlib-only and platform-
neutral; audio and product dependency choices still require review before
promotion into the build.
