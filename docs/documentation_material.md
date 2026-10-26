# Material für Anleitung und Changelog (Arbeitsnotizen)

Stand: 23.10.2026, Branch `add_powder_analysis` (26 Commits vor `main`, alle lokal).
Sammlung für die Dokumentation. Wird nach den Entscheidungen zu Punkt 2–4 in die eigentlichen Dateien überführt.

## Entscheidungen

| # | Frage | Entscheidung |
|---|---|---|
| 1 | Ablage | `manuals/powder_analysis.md` (EN), `manuals/powder_analysis_de.md` (DE), `CHANGELOG.md` im Hauptverzeichnis, Verlinkung aus `README.md` |
| 2 | Zielgruppe | **zwei Dokumente**: Anwender-Anleitung (inkl. Tipps und Fehlerbehebung, Bedienung am Display und über die Weboberfläche getrennt) und Entwickler-Dokument. Anwender-Anleitung = **ein Dokument mit zwei Kapiteln** (Display / Web), gemeinsame Teile einmal; Entwickler-Dokument **nur Englisch** |
| 3 | Bilder | **Vorschaubilder** als Beispielbilder, mit Markierungen für Buttons und Besonderheiten. Web-Screenshots per Headless-Chrome aus der Vorschau möglich (Chrome ist installiert). Projektkonvention: Bilder liegen unter `resources/<thema>/`, z. B. `resources/connect_to_wireless/*.png`, eingebunden mit `![name](../resources/<thema>/<datei>.png)`. Vorschlag: `resources/powder_analysis/` |
| 4 | Umfang Changelog | alles erwähnen (auch Vorschau-Werkzeug, Unit-Tests), aber **möglichst kurz** – nur was sich ändert |

## Stil der bestehenden Anleitungen (`manuals/*.md`)

- Englisch, kurz, Überschrift `# OpenTrickler <Thema>`, nummerierte Schritte, Screenshots aus `resources/`.
- Hinweise als Blockzitat (`> ...`), wichtige Punkte fett.

## README

- Abschnitt `### Operating Modes` (README.md Zeile 45): Liste „Charge Mode“, „Cleanup Mode“ → „Powder Analysis“ ergänzen, mit Link auf die Anleitung.
- Kein vorhandener Changelog im Repository.

## Gliederungsvorschlag Anleitung (EN und DE gleich aufgebaut)

1. **What it does** – lernt ein Profil für ein neues Pulver automatisch an (grob Kp/Kd, fein Kp; fein Kd fest), prüft es mit 10 Würfen, speichert es optional auf einem Profilplatz.
2. **Before you start**
   - Fine Trickler Stop Threshold auf einen Wert **zwischen zwei Waagenschritten** stellen (0.03, 0.05 …), nie genau 0.02/0.04 (Rundung → Stopp erst am Ziel). Coarse Stop Threshold z. B. 3.
   - Freien Profilplatz wählen; Pulverbehälter ausreichend füllen; Dauer ca. 20–30 Würfe (15–25 min inkl. Schale leeren).
3. **Start** – Display: Hauptmenü → Powder Analysis → Next; Web: Portal → Open Powder Analysis bzw. `http://<IP>/powder_analysis`. Display und Web steuern denselben Ablauf.
4. **Setup** – Zielgewicht (vorbelegt mit letztem Ladegewicht), Pulverform (Ball/Stick/Flake/Disc), Körnung (Fine/Medium/Coarse) → Startwerte.
5. **Prepare** – Schale auf die Waage, ggf. nullen; Anzeige der Thresholds, Warnung bei Threshold auf exaktem Waagenschritt.
6. **Flooding** – Start/Pause, Done; beide Rohre füllen.
7. **Throws** – automatischer Ablauf; nach jedem Wurf Schale in den Pulverbehälter leeren und zurückstellen, nächster Wurf startet bei stabiler 0. Pause (mit Zero) nur zwischen Würfen; Abort jederzeit mit Rückfrage.
8. **Phases** (kurz erklärt) – Coarse Kp, Coarse Kd, Fine Kp (kleinster Kp innerhalb 13 s), Verify (10 Würfe, ≥ 9 Treffer; bei 2 Überwürfen Korrektur Kp −10 % oder Mindestdrehzahl −25 %, max. 3).
9. **Result** – 3 Seiten: Grob (Kp/Kd, Zeit, Überwurf), Fein (Kp/Kd, Zeit, Treffer, Grund), Threshold-Empfehlung (gemessenes Nachfallen → empfohlener Wert, erwartete Lage). Speichern: Platz wählen, Name `analysis_<n>` (Web: freier Name, max. 15 Zeichen), „Use this profile now?“.
10. **Web page** – Status, Ergebnis, Live-Diagramm, Protokoll + CSV, Testwurf mit Aufzeichnung (für Fortgeschrittene).
11. **Tips** – aus den Messreihen:
    - Grobes Pulver (z. B. N560): Korn ≈ Toleranz, gelegentliche Überwürfe unvermeidbar; größere Threshold (Empfehlung der Analyse beachten).
    - Threshold-Empfehlung übernehmen, danach ggf. Analyse wiederholen.
    - Analyse nach Firmware-Update oder Pulverwechsel neu durchführen.
    - Thresholds sind global, nicht pro Profil → beim Pulverwechsel prüfen.
12. **Troubleshooting** – Timeout (Pulver fehlt/Rohr leer/Korn klemmt), „Trial limit reached“, „Not verified“, Waage kommt nicht auf 0 (Pause → Zero), Startseite im Web nur aus dem Menü (nicht während Charge Mode).
13. **Configuration** (optional, technisch) – `src/powder_analysis_config.h`: Startwert-Matrix, Zeitziele (10 s grob, 13 s fein, 15 s gesamt), Verifikation, Korrekturen, Waagenauflösung 0.02 gr.

## Fakten für Anleitung/Changelog

- Neue Dateien: `src/powder_analysis.{h,cpp}`, `src/powder_analysis_tuner.{h,cpp}`, `src/powder_analysis_config.h`, `src/html/powder_analysis.html`, `tests/powder_analysis/`, `tools/powder_analysis_preview/`, `docs/powder_analysis_concept.md`.
- Geänderte bestehende Dateien (nur Einbindung + 1 Fix): `src/app.h`, `src/menu.c`, `src/mui_menu.c` (Hauptmenü-Eintrag + Startformular 70), `src/rest_endpoints.c` (Seite + 3 Endpunkte), `src/CMakeLists.txt` (html2header), `src/html/web_portal.html` (Button + Deep Links `/#trickler`, `/#settings`, `/#purge`), `src/charge_mode.cpp` (D-Term-Fix).
- REST: `/powder_analysis`, `/rest/powder_analysis_state`, `/rest/powder_analysis_trials`, `/rest/powder_analysis_trace`.
- Ressourcen: Flash 752 KB (17.9 %), RAM 228 KB (43.4 %) für die gesamte Firmware; Modul ca. +70 KB Flash / +27 KB RAM gegenüber `main`.
- Tests: 748 Prüfungen (`make -C tests/powder_analysis test`); Vorschau: `make -C tools/powder_analysis_preview`.
- Bestätigt mit N135 (Stick/Medium): Analyse 9/10 in 22 Würfen, Charge Mode 11/11 innerhalb ±0.03, 10–14 s pro Wurf.

## Changelog-Entwurf (Stichpunkte)

**Added**
- Powder Analysis: automatic tuning of powder profiles (display menu + web page `/powder_analysis`), start-value matrix by powder shape and grain size, flooding, automatic pan handling, verification with 10 throws, threshold recommendation from the measured after-stop fall, save to a profile slot.
- Web: live status, live chart, trial protocol with CSV export, test throw with per-sample recording (P/D terms, speeds) and CSV export.
- Web portal: button to the powder analysis, deep links `/#trickler`, `/#settings`, `/#purge`.
- Host unit tests for the tuning logic; preview tool for display screens and web page.

**Fixed**
- Charge mode: both tricklers could briefly run at maximum speed at the start of a throw (D term divided by a 0 ms interval). → Upstream-Issue: `docs/upstream_issue_charge_mode_d_term.md` (Link `LINK_TO_BRANCH_OR_COMMIT` noch offen).

**Known limitations**
- Stop thresholds are global (not per profile). A fine threshold on an exact scale step (e.g. 0.02 with 0.02 gr resolution) can make the trickler stop only at the target.
- Fine Kd has almost no effect in this firmware (D term per millisecond) and is not tuned.
- Start values for ball, flake and disc powders are estimates (only stick powders measured).

## Offene Punkte für die Veröffentlichung

- Wohin auf GitHub (Fork, Branch, PR an das Originalprojekt?) – Entscheidung User, nichts wird ohne ausdrückliches OK gepusht.
- Danach Link in `docs/upstream_issue_charge_mode_d_term.md` eintragen.
- Web-Screenshots: aus der Vorschau im Browser oder vom Gerät (User).
