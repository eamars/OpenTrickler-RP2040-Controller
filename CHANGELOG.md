# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Powder Analysis: automatic tuning of a powder profile from the display menu and a web page (`/powder_analysis`). Finds coarse Kp/Kd and fine Kp from start values by powder shape and grain size, verifies with 10 throws, recommends a fine stop threshold from the measured fall after the stop, saves to a profile slot. Manual: [English](manuals/powder_analysis.md), [Deutsch](manuals/powder_analysis_de.md), [developer notes](manuals/powder_analysis_developer.md).
- Web page: live status and chart, trial protocol with CSV export, test throw with per-sample recording (P/D terms, speeds) and CSV export.
- Web portal: button to the powder analysis; the portal opens the tab named in the URL (`/#trickler`, `/#settings`, `/#purge`).
- Host unit tests for the tuning logic (`tests/powder_analysis`) and a preview tool for the display screens and the web page (`tools/powder_analysis_preview`).

### Fixed

- Charge mode: both tricklers could briefly run at maximum speed at the start of a throw (D term divided by a 0 ms interval). See `docs/upstream_issue_charge_mode_d_term.md`.

### Known limitations

- The stop thresholds are global, not per profile; a fine threshold on an exact scale step (e.g. 0.02 with a 0.02 gr scale) can make the trickler stop only at the target.
- Fine Kd has almost no effect in this firmware and is not tuned.
- Start values for ball, flake and disc powders are estimates; only stick powders were measured.
- only tested with A&D scale
