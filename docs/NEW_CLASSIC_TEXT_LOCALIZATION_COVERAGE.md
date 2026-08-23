# New Classic playable-text localization coverage

Status: implementation inventory, 2026-08-22.

The localization objective is every player-visible word in New Classic. It is
split by the source renderer rather than forced into one unsafe file format.
Classic remains a sealed port of the original Japanese game and does not load
any of these packages.

## Runtime text domains

| Domain | Current coverage | Author/runtime artifacts |
|---|---|---|
| New Classic menus and product UI | Title, Option hierarchy, Replay browser/save/viewer, HiScore/online views, Practice, Input/Key/Joy/Device configuration, netplay rooms/profile/status, bounded dynamic templates, and the nine character-selector center-list names | Complete 403-message English and Japanese UTF-8 authors to `T3LNG001`; Japanese compiles to CP932 and mixes real PC-98 machine glyphs with proportional MNUFONT Latin text |
| Executable-resident game text | 61 stage titles/names, Music Room selection titles, registration names, and result labels | Complete English and Japanese UTF-8 `T3GTL-AUTHOR-1` catalogs to CP932 `T3GTL001` |
| Win quotes | All nine character tables, 11 opponent/route blocks per character, three rows per block: 297 exact 58-byte rows | Same `T3GTL001` domain; English rows are merged into the complete catalog and Japanese source rows are in `game-fixed-text-ja-JP.json` |
| Music Room comments | All 19 tracks, one title plus 19 comment rows per track: 380 exact 40-byte rows | Same `T3GTL001` domain and Japanese source catalog |
| Pre/post-round dialogue, Stage 8 dialogue, endings, and extra ending | Every text run in all 28 DM0/DM1/ED scripts, while retaining the source command stream for timing, pictures, palette, and audio | UTF-8 `T3CTP-AUTHOR-1` to `T3CTP001`; complete English and Japanese catalogs and source-free schemas |

The complete compiled game-text catalog therefore contains 738 messages: 61
executable rows + 297 win rows + 380 Music Room rows. Both English and Japanese
fixed catalogs are generated into the executable. The Japanese catalog is
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
replacement string. The remaining image-text migration includes, at minimum:

- original OP/title and Music Room background headings;
- character-select stat sheets and any labels embedded in portraits (the nine
  transparent center-list names are now runtime-localized in New Classic);
- HUD/result/round/lose sheets and stage/win logos;
- Continue, Game Over, registration-rank/background, and staff-credit art;
- text embedded in cutscene pictures rather than emitted by the script VM;
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
  source-ordered 738-message Japanese author catalog.
- `make_cutscene_text_author.py` extracts every script text run without
  rewriting its command stream.
- `th03-game-text-preview` renders all 738 fixed game-text messages through
  the production PC-98 font path into 40 deterministic 640x400 sheets.
- `th03-localized-scene-snapshot` drives all 28 authenticated production
  cutscene scenes and renders one longest-run layout-stress frame per script,
  retaining the real PI background, palette, coordinates, and page operations.
- The compilers freeze source geometry, schema/body digests, locale/fallback
  relationships, encoding, and glyph widths; malformed or cross-locale packs
  fail closed before a scene is published.
