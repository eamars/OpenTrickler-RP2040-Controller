# Releases

Each release has its notes here, written before its tag:
- `<tag>.md`: the release page's text. Its first line is its title here; the page has its own, "OpenTrickler `<tag>`";
- `<tag>_note.txt`: the short note the offer shows when it offers the update (the manifest's `note`), up to 1023 bytes.

A tag is `vX.Y.Z`. The firmware built at it reports `X.Y.Z`, and a build N commits after it `X.Y.Z-N` (`scripts/gen_version.py`). The process is the Open Weapon Mount Computer's, so both are released the same way (ADR-004, gate 3).

A **beta** is a pre-release for users: its notes give its install steps. The **formal release** that follows it is a higher version (v2.0.0 the beta, v2.0.1 or later the formal), so a unit on the beta is offered it over Wi-Fi: the unit offers only a version higher than its own (ADR-004). The release candidates v2.0.0 to v2.0.3 were removed and v2.0.0 given to the v2.0 beta (the owner, 2026-11-29). A unit still on one of them reports a higher version than the beta and is installed again by USB.

How a release is made:
1. Write both files, carrying into them each item of [`next.md`](next.md) (what the changes since the last release ask the notes to say), empty its list, and commit them.
2. Tag that commit (`git tag -a v2.0.0 -m "Firmware v2.0.0"`) and push the tag.
3. The [build workflow](../../.github/workflows/cmake.yml) builds the tag with `scripts/build_release.py --tag`, which refuses a firmware that does not report exactly the tag's version and a tag without its two files. It then drafts a **pre-release** with:
   - the notes, and the files' SHA-256;
   - each file named with the version: each board's full install (`OpenTrickler-<version>-<board>_full_install.uf2`, the first install by USB, also a unit brought back), and the update images (`..._update.bin`: the Pico 2 W's, and the Pico W's two, one for each slot);
   - `manifest.json`, which gives each file's address on the release, and `SHA256SUMS`, under these names in every release.
4. Try it from the draft on each board: `gh release download <tag> --dir <folder>`, then `rc_run.py update --host <address> --manifest <folder>/manifest.json`.
5. Publish the draft, as a pre-release (a beta). GitHub's latest release skips a pre-release, so `releases/latest/download/manifest.json`, the units' default source, does not lead to it. Try it again from its address: `rc_run.py update --host <address> --manifest https://github.com/eamars/OpenTrickler-RP2040-Controller/releases/download/<tag>/manifest.json`.
6. Once a beta is published, its tag is a compatibility floor beside the latest release: `tests/release_records.py` is given it, so every later firmware reads what the beta stored.
7. The formal release is a tag of its own, higher than the beta's, made by steps 1 to 4. Publish it as the latest release: `releases/latest/download/manifest.json` then leads to it, and every unit on the stable channel is offered it.
