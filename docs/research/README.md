# Port research notes

These notes define implementation boundaries for work after the deterministic
gameplay core. They are research inputs, not commitments to a particular UI or
third-party dependency:

- [`PRESENTATION_CONTRACT.md`](PRESENTATION_CONTRACT.md) inventories the
  renderer-facing state and proposes a deterministic raster/command boundary
  plus visual-oracle gates.
- [`ASSET_INVENTORY.md`](ASSET_INVENTORY.md) inventories the extracted TH03
  data without copying original bytes and proposes the `th03_assets` decoder
  boundary.
- [`AUDIO_PMD_YM2608.md`](AUDIO_PMD_YM2608.md) inventories PMD/YM2608 behavior,
  evaluates implementation sources and licenses, and defines an audio proof of
  concept and validation artifacts.

Original game data, reference binaries, and generated PCM/frame captures stay
outside the standalone source repository. Dependency and product choices must
be reviewed before an implementation parcel promotes any candidate into the
build.
