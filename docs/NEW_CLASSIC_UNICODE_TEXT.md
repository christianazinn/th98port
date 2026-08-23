# New Classic Unicode text contract

Status: implemented foundation, 2026-08-23.

New Classic accepts validated UTF-8 for product-shell localization, online
display names, account nametags, and V14 fallback nametag snapshots. Classic
does not read this path: its original Shift-JIS, FONT.ROM, gaiji, coordinates,
and timing remain sealed.

## Rendering order

Text is resolved deterministically, without consulting an installed host font:

1. printable ASCII uses the authenticated proportional `MNUFONT.PF` in menu
   scenes and the authenticated PC-98 FONT.ROM in the MAINL VS title;
2. common Latin diacritics are composed over the matching original Latin glyph
   in menu scenes;
3. existing Japanese CP932 catalogs continue through exact FONT.ROM;
4. all remaining UTF-8 scalars use `T3UFN001.BIN`, compiled from pinned GNU
   Unifont 17.0.05; an absent glyph uses its packaged replacement glyph.

`T3UFN001` is an external 8x16/16x16 glyph package rather than executable data,
so adding script coverage does not bloat the native binary. The compiler stores
ordered scalar IDs, widths, bitmaps, the decompressed source SHA-256, and a body
SHA-256. The runtime rejects wrong ordering, dimensions, reserved Unicode
noncharacters, checksum drift, truncation, and nonzero reserved header bytes.
The package and exact OFL text are installed in beta/release layouts and embedded
standalones.

## Input and layout

The shared UTF-8 decoder rejects overlong encodings, malformed continuations,
surrogates, noncharacters, C0/C1 controls, and truncated scalars. SDL text input
is queued and delivered atomically as UTF-8; numeric and room-code editors still
consume ASCII only. Cursor movement, Backspace, Delete, Home, End, and vertical
line movement preserve scalar boundaries.

Account nametags remain bounded by V14's two 57-byte snapshot slots. Independently,
each MAINL title row is at most 28 display cells. East Asian wide/fullwidth and
emoji ranges occupy two cells, combining marks occupy zero, and words are never
split. The C++ client, V14 reader/writer, signaling contract, and Worker use the
same geometry. The mutable account UUID remains the identity; V14 stores only
the match-time nametag fallback already approved for offline viewers.

Online lists preserve UTF-8 and truncate only at scalar boundaries using the
actual menu-font pixel width. They no longer replace every non-ASCII byte with
`?` or cut a multi-byte sequence in half. The netplay VS intro keeps the source
character-name line and renders each account title independently.

## Localization policy

Semantic New Classic UI—settings, status, advisory text, online/replay fields,
and other information a player must understand—is localizable. A locale may
intentionally retain short stylized English labels that are part of Touhou 3's
established presentation, such as the traditional main-menu commands. That is
a reviewed locale choice, not an accidental untranslated fallback.

Source-constrained `T3GTL001` and `T3CTP001` content remains on its exact
CP932/cell-geometry contract for the complete English and Japanese catalogs.
General-Unicode authoring for those fixed gameplay and cutscene domains is a
separate package-format/layout parcel; this foundation does not reinterpret
their authenticated byte streams. Baked menu and picture text remains the
separate image-decoupling work already recorded in the roadmap.

`localization/es-TEST.json` is only a compiler/rendering fixture for accented
Latin, UTF-8 completeness, and fallback behavior. It is not copied into the
standalone and must not become an official Spanish locale until a native
speaker authors or reviews every string.

GNU Unifont provides deterministic broad glyph coverage, not complex-script
shaping. Scripts that require contextual shaping or bidirectional layout need
a later deterministic shaping contract before they can be advertised as a
supported shipping locale.
