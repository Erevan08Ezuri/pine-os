# Tab5 boot artwork

`pineos-gold-source.png` is the approved gold PineOS artwork, without a baked-in
loading bar. `pineos-gold-tab5.bmp` is its centered, aspect-preserving portrait
crop at the Tab5's native 720 × 1280 resolution, packed as RGB565.

The firmware embeds the BMP, so it needs no SD card or asset installation.
`Tab5Splash` overlays a muted gold progress bar and the bottom-left credit
“Developed by Carter B. bell”. Progress advances after display, storage,
configuration, touch, clock, platform, and shell initialization; completion
is shown briefly before opening home. The decoded splash is freed afterward.
The normal desktop simulator keeps its existing startup sequence.

To regenerate the committed BMP after changing the source image, install Pillow
and run `python assets/splash/prepare.py`. Normal builds do not need Pillow.
