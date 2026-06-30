# SGDK Font Provenance

SegaOS uses SGDK's default bitmap font as its current system font source.

- Upstream: https://github.com/Stephane-D/SGDK
- Pinned reference: `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c`
- Version/tag: `v2.11`
- License: MIT, see `LICENSE.txt`
- Source files inspected: `license.txt`, `res/libres.res`,
  `res/image/font_default.png`, `inc/font.h`, `src/vdp.c`
- Reuse mode: direct-copy, format-converted from SGDK's indexed 8x8 font PNG
  into SegaOS' 1bpp `Glyph` rows in `src/sub/sysfont.c`

The source bitmap is a 16x6 grid of 8x8 tiles covering ASCII 32-127. SegaOS
currently exports ASCII 32-126 through `SysFont_Get()`.
