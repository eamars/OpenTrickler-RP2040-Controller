# Powder analysis preview

Renders every view of the powder analysis with dummy data, so the screens and the web page can be
reviewed without a device:

    make -C tools/powder_analysis_preview
    open tools/powder_analysis_preview/out/index.html

Needs a host C/C++ compiler and python3 (standard library only). Nothing in `src/` is touched:

- **Display screens** (`out/display/*.png`, 128x64 scaled 4x): the module's real drawing functions
  are cut out of `src/powder_analysis.cpp` by `extract_sources.py`, compiled with the u8g2 bitmap
  backend (`library/u8g2/sys/bitmap/common`) and the four fonts the screens use, and rendered for
  each screen state in `display_preview.cpp`.
- **Web page** (`out/web/*.html`): the real `src/html/powder_analysis.html` with a small script
  injected that replaces `fetch()` by canned responses for one scenario. The JSON comes from the
  module's real REST serializers (`json_gen.cpp`), so it matches the device byte for byte. Each file
  opens from disk, polling and the live chart keep working.

`out/` is ignored by git. `make clean` removes the build and the generated files.

## Images for the manual

    make -C tools/powder_analysis_preview doc-images

writes the images of `manuals/powder_analysis.md` / `powder_analysis_de.md` to
`resources/powder_analysis/` (these are committed): the display screens with coloured markings
(rectangles and numbered badges, defined in `doc_images.py`) and screenshots of the scenario pages
taken with headless Google Chrome (`CHROME=<path>` if it is not in `/Applications`; without Chrome
only the display images are written). The screenshots use the banner-free copies in `out/web/doc/`.
