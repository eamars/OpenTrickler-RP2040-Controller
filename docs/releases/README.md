# Releases

Each release has its notes here, written before its tag:
- `<tag>.md`: the release page's text. Its first line is its title here; the page has its own, "OpenTrickler `<tag>`";
- `<tag>_note.txt`: the short note the offer shows when it offers the update (the manifest's `note`), up to 1023 bytes.

A tag is `vX.Y.Z`. The firmware built at it reports `X.Y.Z`, and a build N commits after it `X.Y.Z-N` (`scripts/gen_version.py`). The process is the Open Weapon Mount Computer's, so both are released the same way (ADR-004, gate 3).

Until every ADR is closed, a release is a candidate only: a pre-release, whose notes say it is a test build and give no install steps (the owner, ADR-004).

How a release is made:
1. Write both files and commit them.
2. Tag that commit (`git tag -a v2.0.0 -m "Firmware v2.0.0"`) and push the tag.
3. The [build workflow](../../.github/workflows/cmake.yml) builds the tag with `scripts/build_release.py --tag`, which refuses a firmware that does not report exactly the tag's version and a tag without its two files. It then drafts a **pre-release** with:
   - the notes, and the files' SHA-256;
   - each file named with the version: each board's full install (`OpenTrickler-<version>-<board>_full_install.uf2`, the first install by USB and the way back), its erase (`..._erase.uf2`), and the update images (`..._update.bin`: the Pico 2 W's, and the Pico W's two, one for each slot);
   - `manifest.json`, which gives each file's address on the release, and `SHA256SUMS`, under these names in every release.
4. Try it from the draft on each board: `gh release download <tag> --dir <folder>`, then `rc_run.py update --host <address> --manifest <folder>/manifest.json`.
5. Publish the draft, as a pre-release. GitHub's latest release skips a pre-release, so `releases/latest/download/manifest.json`, the units' default source, does not lead to it. Try it again from its address: `rc_run.py update --host <address> --manifest https://github.com/eamars/OpenTrickler-RP2040-Controller/releases/download/<tag>/manifest.json`.
6. The final is a candidate promoted: untick "Set as a pre-release" and set it as the latest release. Its files are the ones tried; nothing is built again.
