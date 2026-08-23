# New Classic menu backgrounds

These files are repo-owned PoDD Port artwork supplied by the project owner and
generated independently of the original game. They may be redistributed with
native beta and release builds. They contain no extracted PC-98 game assets.

Each screen is stored as two uncompressed 640x200, 4-bpp BMP halves because
the New Classic indexed renderer consumes one 640x400 PC-98-style palette
surface. The runtime joins the `A` (top) and `B` (bottom) files without
resampling. Background pixels deliberately avoid palette indices reserved for
lavender headers, yellow selection, status text, and white body text.

| Pair | Current use |
|---|---|
| `BG2A/B.BMP` | Neutral retained-menu background |
| `M001A/B.BMP` | Replay hub candidate |
| `M002A/B.BMP` | HiScore and online leaderboards |
| `M003A/B.BMP` | Online Profile |
| `M004A/B.BMP` | Unassigned sample |
| `M005A/B.BMP` | Online Match and matchmaking |
| `M006A/B.BMP` | Successful netplay report |
| `M007A/B.BMP` | Failed or forfeited netplay report |

The authoritative runtime mapping is `NewClassicMenuBackground` in
`include/th03/app/new_classic_assets.hpp`. Standalone content stages this
directory as `content/PODDUI`; CMake beta packages install it as `PODDUI`.

New source artwork should be converted and reviewed with
`tools/localization/compile_menu_background.py`; see
`docs/NEW_CLASSIC_MENU_BACKGROUND_AUTHORING.md`. The compiler reserves every
runtime text-palette slot and writes the exact two-half BMP contract without a
DOSBox or PI conversion step.
