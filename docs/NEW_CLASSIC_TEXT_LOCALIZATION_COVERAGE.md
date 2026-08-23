# New Classic playable-text localization coverage

Status: implementation inventory, 2026-08-22.

The localization objective is every player-visible word in New Classic. It is
split by the source renderer rather than forced into one unsafe file format.
Classic remains a sealed port of the original Japanese game and does not load
any of these packages.

## Runtime text domains

| Domain | Current coverage | Author/runtime artifacts |
|---|---|---|
| New Classic menus and product UI | Title, Option hierarchy, Replay browser/save/viewer, HiScore/online views, Practice, Input/Key/Joy/Device configuration, netplay rooms/profile/status, bounded dynamic templates, the nine character-selector center-list names, and the three character-selection stat labels | Complete 406-message English and Japanese UTF-8 authors to `T3LNG001`; Japanese compiles to CP932 and mixes real PC-98 machine glyphs with proportional MNUFONT Latin text |
| Executable/image-resident game text | 61 stage titles/names, Music Room selection titles, registration names, and result labels, five gameplay HUD words, five Continue/Game Over labels, thirteen registration-art labels, and eighteen staff-roll/verdict labels | Complete English and Japanese UTF-8 `T3GTL-AUTHOR-1` catalogs to CP932 `T3GTL001`; source-backed English art is retained where already locale-correct, while New Classic staff credits and translated rows regenerate only audited text regions |
| Win quotes | All nine character tables, 11 opponent/route blocks per character, three rows per block: 297 exact 58-byte rows | Same `T3GTL001` domain; English rows are merged into the complete catalog and Japanese source rows are in `game-fixed-text-ja-JP.json` |
| Music Room comments | All 19 tracks, one title plus 19 comment rows per track: 380 exact 40-byte rows | Same `T3GTL001` domain and Japanese source catalog |
| Pre/post-round dialogue, Stage 8 dialogue, endings, and extra ending | Every text run in all 28 DM0/DM1/ED scripts, while retaining the source command stream for timing, pictures, palette, and audio | UTF-8 `T3CTP-AUTHOR-1` to `T3CTP001`; complete English and Japanese catalogs and source-free schemas |

The complete compiled game-text catalog therefore contains 779 messages: 61
executable rows + 297 win rows + 380 Music Room rows + five HUD image-text
labels + five Continue/Game Over labels + thirteen registration-art labels +
eighteen staff-roll/verdict labels. Both English and Japanese fixed catalogs
are generated into the executable. The Japanese catalog is
byte-for-byte sourced from the authenticated original executables and DAT rows,
while its readable UTF-8 author file can be rebuilt without transcription.

Normal Story, VS, replay playback, Music Room, and netplay post-match use the
same selected locale catalog. Source bytes are still authenticated and parsed
before substitution, so localization cannot hide a malformed or mismatched
installation.

## Text still baked into graphics

Some visible lettering is not a string in ZUN's data. It is pixels or glyph
sheets inside PI/CDG/BFNT resources. Those surfaces cannot honestly be called
runtime-localizable until their owner restores the underlying art and draws a
replacement string. New Classic already reconstructs every semantic
staff-credit and verdict cel from the selected locale after authenticating its
source art. The original geometry, alpha ownership, slide/dissolve schedule,
and decorative STF8 character illustration remain unchanged. Remaining
image-text migration includes, at minimum:

- original OP/title background headings. The Music Room heading is now
  runtime-localized in New Classic: the source picture is authenticated, its
  flat top-right title panel is reconstructed, and the active T3LNG001 title
  plus creator credit are rendered through the production PC-98 font. Classic
  retains the untouched source picture. KeyConfig, JoyConfig, Device
  Assignment, and Input Test no longer show BG1.PI's baked English heading:
  the four New Classic-only scenes share the project-owned neutral background
  and render both English and Japanese headings at runtime. BG1 remains an
  authenticated legacy archive member for compatibility, but the native asset
  owner no longer decodes or retains it;
- character portrait art (the audited selection atlases contain no text; the
  nine transparent center-list names and all three stat-sheet labels are now
  runtime-localized in New Classic);
- stage/win logos and other gameplay art (the `LOSE.BF2` and `ROUND.BF2` word
  cels are now runtime-localized while their digits and animation stay source-owned);
- text embedded in cutscene pictures rather than emitted by the script VM.
  The canonical VM inventory proves that all 24 pictures are reachable. Four
  current English pictures contain bounded raster translations, while at least
  nine more contain handwritten notes, signage, logos, punctuation, or sound
  effects. `NEW_CLASSIC_PICTURE_TEXT_LOCALIZATION.md` specifies and documents
  the implemented source-bound `T3PTP001` sparse indexed-image compiler,
  parser, and New Classic scene application rather than unsafe generic-font
  reconstruction; reviewed localized overlays remain content work;
- the one-time ZUN Soft logo and other logo/credit artwork.

These are deliberately classified as image-text work rather than silently
placing untranslated pixels in a string census. Each future migration needs a
locale-neutral backing asset, an explicit layout/width contract, production
renderer ownership, and scene snapshots. Original-art dependencies remain
external; replacement localization assets may be distributed with the port.

Installation manuals and other text files with no playable TH03 loader are not
game text and are excluded. Replay/player names, dates, scores, device names,
and online nametags are runtime data rather than translatable strings; their
labels and formatting templates are included in the menu domain.

## Reproducibility and review

- `make_fixed_game_text_author.py` extracts and validates the Japanese or
  English fixed DAT tables and can merge them into a full author catalog.
- `make_japanese_game_text_author.py` authenticates original Japanese
  OP/MAINL/MAIN, extracts their remaining 61 records, and produces the complete
  source-ordered 779-message Japanese author catalog.
- `make_cutscene_text_author.py` extracts every script text run without
  rewriting its command stream.
- `th03-game-text-preview` renders all 779 fixed game-text messages through
  the production PC-98 font path into 45 deterministic 640x400 sheets.
- `th03-localized-scene-snapshot` drives all 28 authenticated production
  cutscene scenes and renders one longest-run layout-stress frame per script,
  retaining the real PI background, palette, coordinates, and page operations.
- `extract_cutscene_picture_uses()` drives the same canonical VM and inventories
  full-background, immediate-quarter, and crossfade-quarter presentations. Its
  authenticated Japanese and English gates reach all 24 cutscene pictures;
  case-insensitive member identity is normalized only after the VM parses it.
- The compilers freeze source geometry, schema/body digests, locale/fallback
  relationships, encoding, and glyph widths; malformed or cross-locale packs
  fail closed before a scene is published.
