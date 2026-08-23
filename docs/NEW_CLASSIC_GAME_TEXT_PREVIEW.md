# New Classic game-text preview

`th03-game-text-preview` renders all 761 source-constrained `T3GTL001`
messages without launching gameplay. It uses the production `FONT.ROM`
provider and graphics-text preparation/blit path, so the output is a review of
the same CP932 bytes, glyphs, widths, and pixels used by New Classic.

```powershell
out/build/gcc/th03-game-text-preview.exe `
  --font path/to/FONT.ROM `
  --output out/game-text-preview `
  --game-text-pack path/to/locale.t3gtl
```

Omit `--game-text-pack` to render the built-in English catalog. The output is:

- `music.ppm`, containing all 19 Music Room titles;
- `stage.ppm`, containing the 18 stage-title lines and nine names;
- `registration-result.ppm`, containing ten registration names and five result
  labels;
- `hud-banners.ppm`, containing the five gameplay HUD word labels;
- `continue-gameover.ppm`, containing the five full-image Continue/Game Over
  labels;
- `registration-art.ppm`, containing the title, columns, difficulties, edit
  commands, and all-stage marker removed from registration image resources;
- 18 win-quote pages (two for each character) and 19 Music Room comment pages;
- `preview.json`, a deterministic `T3GTP001` index with every stable message
  ID, page, bounding rectangle, and exact source byte count.

Each image is an unscaled 640x400 binary PPM. The tool creates no outputs if
the font or localization package fails validation. The preview is an authoring
aid, not an alternate renderer: layout changes still belong in the production
owner and therefore change this output through the shared path.
