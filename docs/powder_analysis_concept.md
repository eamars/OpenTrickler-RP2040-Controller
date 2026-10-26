# Pulveranalyse (Powder Analysis) – Konzept

Status: **Review abgeschlossen (Schritt 3)**
Branch: `add_powder_analysis`

---

## 1. Zweck

Das Anlernen eines Profils für eine neue Pulversorte soll vereinfacht werden. Bisher startet man mit
beliebigen Werten, beobachtet das Verhalten, passt an und wiederholt das – ein langer, mühsamer Prozess.

Die Pulveranalyse führt dieses Vorgehen **automatisch** nach dem offiziellen
[Tuning Guide](https://github.com/eamars/OpenTrickler/blob/main/Manual/tuning_guide.md) durch,
zeigt am Ende einen Vorschlag für die Profilwerte an und kann diesen optional auf einem Profilplatz speichern.

## 2. Rahmenbedingungen

- Die Funktion wird als **eigenständiges Modul** umgesetzt und nutzt nur den vorhandenen Code.
- Änderungen am vorhandenen Code beschränken sich auf die **Einbindung**: Eintrag im Display-Menü,
  Registrierung der REST-Endpunkte, Einbindung der neuen Web-Seite in den Build und Link im Web-Portal.
- Die Profile im EEPROM werden **nur beim bewussten Speichern am Ende** verändert.
- Globale Charge-Mode-Einstellungen (z. B. `coarse_stop_threshold`) werden **nur gelesen, nie geändert**.
- Ressourcen des Pico schonen: Diagramme und CSV-Export entstehen im Browser, nicht auf dem Gerät.

## 3. Umfang

### Ermittelt werden

| Wert | Ermittlung |
|---|---|
| `coarse_kp` | automatische Suche |
| `coarse_kd` | Sondierung (Abschnitt 4.6) |
| `fine_kp` | automatische Suche |
| `fine_kd` | fest aus der Startwert-Matrix (Abschnitt 4.8) |
| `coarse_ki`, `fine_ki` | immer `0` (laut Tuning Guide) |

### Fest vorgegeben (keine Suche, um Motor-Stall zu vermeiden)

| Wert | Grob | Fein |
|---|---|---|
| `min_flow_speed_rps` | 0.1 | 0.06 als Startwert, wird nur von der Verifikation gesenkt (Abschnitt 4.9) |
| `max_flow_speed_rps` | 5.0 | 5.0 |

### Bewusst nicht im Umfang

- Ändern von `coarse_stop_threshold` / `fine_stop_threshold`. Für `fine_stop_threshold` gibt es nur eine Empfehlung
  aus dem gemessenen Nachfallen (Abschnitt 4.10a).

## 4. Gesamtablauf

```
 Setup ──► Vorbereitung ──► Fluten ──► Schale leeren
                                            │
                                            ▼
                                  Phase A: Grob-Kp suchen
                                            │
                                            ▼
                                  Phase B: Grob-Kd suchen
                                            │
                                            ▼
                                  Phase C: Fein-Kp suchen (Fein-Kd fest aus der Matrix)
                                            │
                                            ▼
                                  Phase D: Verifikation 9/10
                                            │
                                            ▼
                                  Ergebnis ──► (optional) Speichern
```

Zwischen **jedem** Versuch: „Schale in den Pulverbehälter leeren und zurückstellen“.
Der nächste Versuch startet automatisch, sobald die leere Schale stabil 0 zeigt.

### 4.1 Setup (Benutzereingaben)

1. **Zielgewicht** (in der Einheit der Waage, Eingabe wie bei „Select Charge Weight“)
2. **Pulverform**: Ball / Stick / Flake / Disc
3. **Körnung**: Fine / Medium / Coarse

Form + Körnung wählen die **Startwerte** aus der Matrix (Abschnitt 6).

### 4.2 Vorbereitung

- Anzeige der aktuell eingestellten Stop-Thresholds aus der Charge-Mode-Konfiguration
  (z. B. „Coarse stop 3.0 / Fine stop 0.02“). Die Analyse tuned **auf diese Werte**; sie sind global
  (nicht pro Profil) und müssen ggf. vorher in den Charge-Mode-Einstellungen angepasst werden.
- Hinweis: Waagschale auf die Waage stellen und bei Bedarf selbst nullen. Weiter mit „Next“.

### 4.3 Fluten

- Beide Trickler laufen gleichzeitig mit fester Drehzahl: **grob 1.0 rps, fein 2.0 rps** (anpassbare Konstanten).
- Ist ein Servo-Gate in der Konfiguration aktiviert, wird es **zu Beginn der Analyse einmal geöffnet und bleibt offen**
  (auch nach Ende/Abbruch). Precharge wird nicht verwendet.
- Der User steuert mit **Start / Pause** (umschaltbar) und **Done** (Rohre sind voll).
- Pause stoppt beide Motoren, damit die Schale geleert werden kann.
- Nach „Done“: Schale leeren und zurückstellen → bei stabil 0 beginnt Phase A.

### 4.4 Ein Versuch (Testwurf)

Ein Versuch ist ein Wurf, der sich so verhält wie im normalen Charge Mode, aber mit den **Testwerten**
statt der Werte aus dem gewählten Profil:

- Regelung identisch zum Charge Mode (PID, Grob stoppt bei `Ziel − coarse_stop_threshold`,
  Fein stoppt bei `Ziel − fine_stop_threshold`).
- **Coarse-Backoff** wird wie im Charge Mode angewendet (Einstellungen aus der Charge-Mode-Konfiguration).
- **Servo-Gate** bleibt offen (siehe 4.3), **Precharge** wird ignoriert.
- **LED-Farben** wie im Charge Mode (Unter-/Über-/Treffer-Farbe aus der Charge-Mode-Konfiguration).
- Nach dem Stopp wird gewartet, bis das Gewicht **stabil** ist, dann wird gemessen.
- Gespeichert werden: Phase, Kp, Kd, Endgewicht, Dauer Grobphase, Gesamtdauer, Bewertung.
- **Sicherheits-Timeout** pro Versuch: 30 s (ein Wurf mit zu kleinem Kp wird abgebrochen und als Timeout gewertet,
  Gewicht und Zeiten sind die dieses Moments).
- **D-Anteil beim ersten Messwert**: der D-Anteil ist beim ersten Messwert und bei einem Zeitabstand von 0 gleich 0.
  Vorher wurde durch die verstrichene Zeit geteilt, die im ersten Schleifendurchlauf oft 0 ist (ein Messwert lag
  schon bereit) – Ergebnis inf/NaN, beide Trickler sprangen bis zum nächsten Messwert auf Maximaldrehzahl
  (Messreihe N320: 13 von 26 Würfen nach 1.6 s mit 12–13 gr). Der Fehler ist auf Entscheidung des Users auch im
  Charge Mode behoben (gleiche kleine Hilfsfunktion in `charge_mode.cpp`).
- Jede Bracketing-Suche verdoppelt höchstens 6-mal (Startwert × 64) und halbiert höchstens 5-mal: ein Parameter
  ohne Wirkung kann so nicht endlos verdoppelt werden; die Suche endet dann mit ihrem besten Wert.

**Schale leeren zwischen den Versuchen** (wie im Charge Mode):
1. Warten, bis die Schale **abgenommen** wurde (Gewicht fällt deutlich).
2. Warten, bis die Schale **zurück** ist und das Gewicht stabil bei 0 liegt
   (gleiche Kriterien wie Charge Mode: `set_point_sd_margin` / `set_point_mean_margin`).
3. Nächster Versuch startet automatisch – außer der User hat **Pause** gedrückt.

### 4.5 Phase A – Grob-Kp

- Grob: Kp = Startwert, Ki = 0, Kd = 0. Fein läuft mit Kp = Ki = Kd = 0 (also Mindestdrehzahl) mit,
  wie im realen Betrieb.
- Gemessen wird das Gewicht **nach dem Grob-Stopp** (der Versuch endet nach der Grobphase).
- Bedingung erfüllt, wenn **beides** gilt:
  - **Überwurf**: Ergebnis liegt über dem Grob-Stoppunkt (`Ziel − coarse_stop_threshold`) **und**
  - **schnell genug, um Platz für die Bremse zu lassen**: Grobzeit ≤ 10 s / Bremsfaktor
    (Bremsfaktor 1.3 → ≤ 7.7 s).
- Beide Bedingungen wachsen mit Kp (mehr Kp = mehr Überwurf und schneller), die Suche bleibt eindeutig:
  1. Bedingung nicht erfüllt → Kp **verdoppeln**.
  2. Erste Erfüllung → **binäre Suche** zwischen letztem Wert ohne und erstem Wert mit Erfüllung.
  3. Ergebnis: der **kleinste Kp**, der die Bedingung erfüllt, und seine Grobzeit ohne Bremse (**Basiszeit**).
- Ist schon der Startwert erfüllt, wird halbiert, bis er es nicht mehr ist.
- Abbruch der Bisektion, wenn das Intervall < 15 % ist oder nach max. 3 Bisektionsschritten.

### 4.6 Phase B – Grob-Kd (sanfte Bremse)

Kd soll den Grob-Trickler zum Schluss **spürbar, aber begrenzt** abbremsen, damit nicht zu viel Pulver auf
einmal nachfällt und der Grobwurf konsistenter wird. Kd = 0 ist nicht erwünscht.

- Kp aus Phase A bleibt fest, Kd = Startwert.
- **Keine Bracketing-Suche** (Erkenntnis aus dem Hardwaretest): der D-Anteil arbeitet mit der Fehleränderung
  pro Millisekunde (`charge_mode.cpp`), daher ändert Grob-Kd im üblichen Bereich die Grobzeit kaum, und die
  gemessenen Grobzeiten streuen stärker (±2 s), als Kd sie verschiebt. Stattdessen wird **sondiert**:
  - Startwert, dann verdoppeln, maximal 5 Verdopplungen (0.1 … 3.2).
  - Vorzeitig Schluss, sobald sich die Grobzeit gegenüber dem vorigen Kd um weniger als 10 % ändert
    (Plateau) oder ein Wurf das **Zeitziel von 10 s** überschreitet. (Ein Budget „Basiszeit × Bremsfaktor“
    wurde nach Messreihe 2 verworfen: die Basiszeit stammt aus einem einzelnen Wurf, und Grobzeiten gleicher
    Werte streuen zwischen ≈2 s und ≈8 s – das Budget war damit Zufall.)
  - Ergebnis: von allen Kd innerhalb von 10 s der mit der **kürzesten Grobzeit**; Zeiten innerhalb von
    0.2 s der kürzesten gelten als gleich, davon gewinnt der **kleinste Kd**.
  - Die Phase muss mit einem Wurf innerhalb von 10 s enden: war schon der Startwert zu langsam, wird er bis zu
    2-mal wiederholt (Streuung). Erst wenn auch das nichts bringt: kleinster probierter Kd mit Vermerk im Ergebnis.
- Kontrolle: Überwurf über den Stoppunkt des gewählten Kd muss ≤ 1.0 gr sein. Wird das nicht erreicht,
  wird es im Ergebnis vermerkt.
- **Bremsfaktor** (Konfigurationswert 1.3) wird nur noch in Phase A verwendet (Grobzeit ≤ 10 s / 1.3 lässt
  Platz für die Bremse).

### 4.7 Phase C – Fein-Kp

- Grob mit den Werten aus A + B, Fein: Kp = Startwert, Ki = 0, **Kd = Startwert aus der Matrix** (Abschnitt 4.8).
- **Realistische Würfe**: erst grob, dann fein bis zum Ziel.
- Gesucht wird der **kleinste Fein-Kp, dessen voller Wurf innerhalb von 13 s fertig ist** (Konfigurationswert
  `POWDER_ANALYSIS_FINE_TARGET_TIME_S`; Bracketing-Suche „kleinster erfüllter Wert“): Bedingung erfüllt = kein Timeout
  und Gesamtzeit ≤ 13 s. Ein Überwurf spielt für die Bedingung keine Rolle – ein kleinerer Kp senkt ihn nur, den Rest
  erledigt die Korrektur der Verifikation. Der Startwert aus der Matrix ist meist schnell genug → halbieren, bis zu
  langsam, dann Bisektion. Die Bewertungen (HIT/OVER/SLOW/…) bleiben im Protokoll.
- Warum der langsamste noch schnell genuge Kp: beim Fein-Stopp ist der Fehler ≈ `fine_stop_threshold`, die Drehzahl beim
  Stopp also `max(Kp × Threshold, Mindestdrehzahl)`, und das Nachfallen wächst mit dieser Drehzahl (Messreihe N135-5:
  Kp 6.0 → +0.156 gr, 4.5 → +0.056, 3.0 → 0…+0.028). Die 2 s Abstand zum 15-s-Ziel fangen die Streuung der Grobzeit
  (4.4–8.4 s) ab; der User akzeptiert bis ≈ 15 s für mehr Präzision.
- Der Wurf mit dem gefundenen Kp liegt damit unter 13 s; war er es nicht (nur bei Abbruch der Suche möglich), wird das
  im Ergebnis vermerkt.

### 4.8 Fein-Kd – fest aus der Matrix (keine Suche)

- Fein-Kd wird **nicht gesucht**; der Wert kommt aus der Startwert-Matrix (Abschnitt 6) und bleibt die ganze Analyse
  über gleich (auch in der Fein-Kp-Suche und der Verifikation).
- Grund (Messung 9, Testwurf mit Aufzeichnung, N320, Fein-Kp 4.8, Kd 0 … 64): der D-Anteil arbeitet mit der
  Fehleränderung pro Millisekunde, und die Waage ändert ihren Wert nur jeden zweiten Messwert in 0.02-gr-Schritten.
  Kurz vor dem Stopp beträgt der D-Anteil < 1 % der Fein-Drehzahl bei Kd 4, 2–4 % bei Kd 16 und erst 10–17 % bei
  Kd 64 – und er springt zwischen 0 und einem Wert hin und her. Alle sechs Würfe liefen am Ende mit derselben
  Drehzahl (Kp × Restabweichung) und endeten gleich. Das Bremsen am Ende macht der P-Anteil allein; eine Kd-Suche
  würde nur dem Rauschen folgen (Messreihe 4: bis Kd 128, Würfe 14–18 s).

### 4.9 Phase D – Verifikation

- Eine **Runde** = bis zu 10 Würfe mit **festen Werten**; innerhalb einer Runde wird nichts verändert, sie testet nur.
  Treffer = Ziel ± `fine_stop_threshold` und Gesamtzeit < 15 s.
- Eine Runde hat immer **10 Würfe**; **≥ 9/10 Treffer** (Konfigurationswerte) → verifiziert, fertig. Ein einzelner
  Pulverklumpen nach dem Stopp soll ein gutes Profil nicht durchfallen lassen; fünf Würfe hatten seltene große
  Nachfälle übersehen, die dann im Charge Mode auftraten (N135-5: 4 von 13 Würfen über). Angezeigt wird die echte Zahl
  („Verified 9/10“ / „Verified 10/10“).
- Sobald eine Runde **2 Überwürfe** hat, wird sie abgebrochen und korrigiert, neue Runde. Überwürfe in der
  Verifikation sind einzelne Körner, die nach dem Stopp noch fallen; was fällt, bestimmt die Drehzahl im Moment des
  Stopps = **max(Fein-Kp × `fine_stop_threshold`, Fein-Mindestdrehzahl)** (D-Anteil vernachlässigbar, Abschnitt 4.8).
  Die Korrektur senkt den Term, der diese Drehzahl bestimmt: ist Kp × Threshold größer als die Mindestdrehzahl →
  **Fein-Kp × 0.9**, sonst **Fein-Mindestdrehzahl × 0.75** (0.06 → 0.045 → 0.034 → 0.025 rps). Maximal 3 Korrekturen
  insgesamt; die gesenkten Werte landen im gespeicherten Profil.
- Endet eine Runde mit weniger als 2 Überwürfen, aber unter 9/10 (zu langsam, zu leicht, Timeout, 1 Überwurf) →
  Ende, nicht verifiziert, Anzeige x/10. Keine Korrektur.
- Scheitert auch die Runde nach der 3. Korrektur an Überwürfen → Ende mit der **besten Runde** (meiste Treffer,
  bei Gleichstand die schnellere) und deren Werten.
- Protokoll und Anzeige machen Korrekturen sichtbar: die Fein-Mindestdrehzahl jedes Wurfs steht im Protokoll
  (CSV-Spalte `fine_min_speed_rps`), nach einer Korrektur zeigt D8 „Kp -10% (n/3)“ bzw. „MinSpd -25% (n/3)“, der
  Web-Status zeigt Runde und Treffer („Round 2: 3/10 hits“).

### 4.10 Abbruchkriterien und „bestes Ergebnis“

- Gesamt-Obergrenze: **60 Würfe**. Wird sie erreicht, endet die Analyse mit dem bisher besten Stand; das Ergebnis
  vermerkt es (`trial_limit_reached`).
- Wird ein Zeitziel (10 s / 15 s) oder die Verifikation nicht erreicht, wird **das bestmögliche Ergebnis**
  angezeigt, mit Hinweis, was nicht erreicht wurde.
- „Bestes Ergebnis“ in der Verifikation = die **beste Runde**: die mit den meisten Treffern, bei Gleichstand die mit
  der kürzeren mittleren Gesamtzeit; ihre Werte (Fein-Kp, Fein-Mindestdrehzahl) werden das Ergebnis.

### 4.10a Nachfallen nach dem Stopp und Threshold-Empfehlung

- Jeder volle Wurf merkt sich das Gewicht des Messwerts, der den Fein-Stopp auslöst. **Nachfallen** = stabiles
  Endgewicht − dieses Stoppgewicht; es steht pro Wurf im Protokoll (Web-Spalte „Fall gr“, CSV `fall_after_stop_gr`;
  leer bei reinen Grobwürfen, Timeouts und Abbrüchen).
- Ergebnis: Median und Maximum des Nachfallens über die Verifikationswürfe (ersatzweise die Fein-Kp-Würfe) und daraus
  eine **Empfehlung für `fine_stop_threshold`**. Die Waage liefert Vielfache ihrer Auflösung (0.02 gr, Konfigurationswert
  `POWDER_ANALYSIS_SCALE_RESOLUTION_GR`), und der Charge Mode stoppt bei `Ziel − Gewicht < Threshold`. Ist der Threshold
  selbst ein Vielfaches der Auflösung, entscheidet Float-Rundung, ob eine Stufe früher oder erst am Ziel gestoppt wird
  (Messreihe N135-4: 23.1 − 23.08 = 0.0200005 → nicht < 0.02 → Stopp erst bei 23.100, jedes Nachfallen ein Überwurf;
  bei 8.2 dagegen 0.0199995 → Stopp bei 8.18). Deshalb: n = Nachfallen-Median / Auflösung, kaufmännisch gerundet,
  mindestens 1; **Empfehlung = (n + 0.5) × Auflösung** (0.03, 0.05, 0.07 …). Damit stoppt der Charge Mode zuverlässig bei
  `Ziel − n × Auflösung`, akzeptiert dieselben angezeigten Werte, und die Würfe landen im Mittel bei
  `Ziel − n × Auflösung + Median` (als `expected_final_offset_gr` mitgeliefert). Nur ein Hinweis auf der Web-Ergebnisseite
  (mit Warnung, wenn der eingestellte Threshold ein exaktes Vielfaches der Auflösung ist) – die globale Einstellung wird
  **nicht** geändert.

### 4.11 Pause und Abbruch

- **Pause** wirkt **zwischen** zwei Würfen (beim Schritt „Schale leeren“). Ein laufender Wurf wird nicht
  unterbrochen, sonst wäre die Messung wertlos. Bei Pause: Hinweis, ggf. Pulver nachzufüllen.
  Im Pause-Bildschirm kann die Waage bei Bedarf manuell genullt werden („Zero“, nur wenn der Waagentreiber
  das unterstützt); automatisch wird nie genullt, da Pulverreste in der Schale sonst „weggenullt“ würden.
- **Abort** ist jederzeit möglich, mit **Sicherheitsabfrage** („Abort analysis? Yes / No“).
  Motoren stoppen sofort, der Fortschritt geht verloren.

### 4.12 Ergebnis und Speichern

- Anzeige der ermittelten Werte (grob + fein), Zeiten und Treffer x/10.
- **Save**: Profilplatz wählen (Liste mit den vorhandenen Namen) → Sicherheitsabfrage „Overwrite ‚…‘?“.
- Name:
  - **Display**: automatisch `analysis_<n>` (n = kleinste noch nicht vergebene Nummer unter den Profilnamen).
  - **Web**: frei eingebbar (max. 15 Zeichen), Vorbelegung `analysis_<n>`.
- Gespeichert werden alle 10 Profilwerte (inkl. fester Flow-Speeds, Ki = 0).
- Nach dem Speichern: Abfrage **„Use this profile now?“ (Yes / No)**. Nur bei Yes wird das Profil als aktives Profil ausgewählt.
- **Discard**: Werte verwerfen, zurück ins Hauptmenü.

## 5. Benutzeroberflächen

Display und Web bedienen **denselben Ablauf parallel** (wie beim Charge Mode). Die Ablauflogik liegt
im Modul; beide Oberflächen zeigen nur den Zustand und senden Bedienbefehle.
Alle Texte auf Englisch (wie die bestehende Oberfläche).

### 5.1 Display (128×64, Drehgeber + Taste)

Neuer Eintrag im Hauptmenü: **„Powder Analysis“** (nach „Cleanup“).

| # | Bildschirm | Inhalt | Bedienung |
|---|---|---|---|
| D1 | Target Weight | Zielgewicht eingeben | Back / Next |
| D2 | Powder Shape | Ball / Stick / Flake / Disc | Back / Next |
| D3 | Grain Size | Fine / Medium / Coarse | Back / Next |
| D4 | Prepare | Thresholds (coarse/fine), „Put pan on scale, zero scale if needed“. Liegt der Fein-Threshold genau auf einer Waagenstufe (Abschnitt 4.10a): Markierung „!“ und Tipp „F: use 0.03, 0.05..“ | Back / Next |
| D5 | Flooding | Gewicht, „Running“/„Paused“ | Start/Pause, Done, Abort |
| D6 | Empty Pan | „Empty pan into hopper and put it back“ + Gewicht | Pause, Abort |
| D7 | Trial running | Phase (z. B. „Coarse Kp“), Versuch Nr., Kp/Kd, Gewicht / Ziel (z. B. „14.82 / 23.10 gr“), Zeit | Abort |
| D8 | Trial result | z. B. „35.6gr 7.2s OVER“, darunter D6-Hinweis | Pause, Abort |
| D9 | Paused | „Paused – refill hopper if needed“ + Gewicht | Resume, Zero (nur wenn der Waagentreiber nullen kann), Abort |
| D10 | Abort? | „Abort analysis?“ | No / Yes |
| D11 | Result 1/3 | Grob: Kp, Kd, Zeit und Überwurf über den Grob-Stoppunkt („4.1s over 0.55gr“), Hinweis | Next |
| D12 | Result 2/3 | Fein: Kp, Kd, Zeit, Treffer x/10 (+ ggf. Hinweis) | Next |
| D13 | Result 3/3 | Threshold: empfohlener Fein-Threshold („Rec. fine thr 0.03“), erwartete Landung („Lands ~ -0.004 gr“), aktuelle Einstellung („Current 0.02“); ohne Nachfall-Daten „No fall data“ | Save / Discard |
| D14 | Save slot | Profilplatz wählen (mit Namen) | Back / Next |
| D15 | Confirm | „Overwrite ‚…‘ as analysis_n?“ | Back / Save |
| D16 | Activate | „Use this profile now?“ | No / Yes |

### 5.2 Web (eigene Seite `powder_analysis.html`)

Eigene Seite, im Web-Portal nur verlinkt. Keine externen Bibliotheken (funktioniert auch im Access-Point-Modus ohne Internet).

1. **Setup**: Zielgewicht, Pulverform, Körnung, Buttons Start / Pause / Done (Fluten) / Abort
2. **Live-Status**: Phase, Versuch Nr., aktuelle Kp/Kd, aktuelles Gewicht, Aufforderungen an den User
3. **Ergebnis** (direkt unter dem Status, sobald ein Ergebnis vorliegt): Werte, Threshold-Hinweis,
   Profilplatz-Auswahl, freier Name, Speichern / Verwerfen
4. **Live-Diagramm** (aufklappbar): Gewichtsverlauf des aktuellen Wurfs, selbst gezeichnet (Canvas).
   Die Kurve wird im Browser aus der ohnehin laufenden Statusabfrage gesammelt.
5. **Protokoll**: Tabelle aller Versuche (Nr., Phase, Kp, Kd, Endgewicht, Zeit grob, Zeit gesamt, Bewertung,
   Fein-Mindestdrehzahl, Nachfallen) + **CSV-Download** (im Browser erzeugt)
6. **Testwurf** (aufklappbar, ganz unten, Abschnitt 5.3)

Das Protokoll liegt im RAM des Geräts (max. 60 Einträge, wenige KB), damit die Seite auch mitten in der
Analyse geöffnet oder neu geladen werden kann.

### 5.3 Testwurf mit Aufzeichnung (nur Web)

Zweck: messen, was P- und D-Anteil des Fein-Tricklers tatsächlich zur Motordrehzahl beitragen (bei allen drei
Pulvern hatte Fein-Kd keinen messbaren Einfluss auf das Endgewicht). Dafür wird **jeder Wurf** (Analyse und
Testwurf) pro Waagen-Messwert aufgezeichnet: Zeit seit Wurfstart, Gewicht, Fein-P, Fein-D, kommandierte
Fein-Drehzahl (nach Min/Max-Begrenzung), Grob-P, Grob-D, kommandierte Grob-Drehzahl (0 nach dem Stopp, negativ
während des Backoffs). Puffer: 300 Messwerte (≈ 30 s bei 10 Hz, ≈ 9.6 KB RAM); längere Würfe werden
abgeschnitten (`truncated`). Auch der Messwert, der den Stopp auslöst (Fein-Stopp, Ende des reinen Grobwurfs,
Timeout), wird aufgezeichnet – mit den P/D-Anteilen dieses Moments und Drehzahl 0, da die Motoren dann stehen.
Die Aufzeichnung des letzten Wurfs bleibt bis zum nächsten Wurf erhalten.

- **Testwurf**: Web-Kommando `TEST_THROW` mit Zielgewicht, Grob-Kp/Kd, Fein-Kp/Kd und Fein-Mindestdrehzahl.
  Nur möglich, wenn das Gerät im Hauptmenü ist oder gerade einen Testwurf beendet hat; Ablauf wie ein Analysewurf
  (Nullwarten bzw. Schale leeren, voller Wurf mit Backoff, stabiles Gewicht), danach „Test throw done“ mit
  Gewicht, Zeit und Bewertung gegen `fine_stop_threshold`. Das Display zeigt nur den Ablauf (Titel „Test throw“);
  Abbruch wie in der Analyse. Tuner und Analyseergebnis bleiben unberührt.
- **Datenformat**: `/rest/powder_analysis_trace?from=<index>` liefert bis zu 40 Messwerte ab `from`:
  `{"id":<Wurfnummer>,"n":<Anzahl>,"tr":<abgeschnitten>,"from":<index>,"p":{tw,ckp,ckd,fkp,fkd,fms,fw},`
  (`fw` = stabiles Endgewicht nach dem Wurf, `null` solange unbekannt oder bei Abbruch),
  "s":[[t_s,weight,fine_p,fine_d,fine_speed,coarse_p,coarse_d,coarse_speed],…]}`. Der Status enthält `tid`
  (Nummer des letzten aufgezeichneten Wurfs); die Web-Seite holt die Aufzeichnung in Stücken, sobald `tid`
  sich ändert, behält die letzten 6 Würfe und zeigt pro Wurf Fein-Kp/Kd, Mindestdrehzahl, Endgewicht, Zeit,
  Fein-Drehzahl beim Stopp, max. |Fein-D| in der Feinphase und max. Fein-P am Ende; dazu ein Diagramm
  (Gewicht links, Fein-P/D/Drehzahl rechts) und CSV-Export pro Wurf (`t_s,weight_gr,fine_p_rps,fine_d_rps,
  fine_speed_rps,coarse_p_rps,coarse_d_rps,coarse_speed_rps`, davor `final_weight_gr`) sowie für alle Würfe in
  einer Datei (`throw,coarse_kp,coarse_kd,fine_kp,fine_kd,fine_min_speed_rps,target_gr,final_weight_gr` + dieselben
  Spalten, pro Zeile wiederholt).

## 6. Startwert-Matrix

Gut kommentierte, leicht anpassbare Tabelle in der Header-Datei des Moduls.

**Alle Werte sind Schätzungen.** Die vorhandenen Standardprofile (AR2208, AR2209, 8208XBR, Benchmark2 –
alles Stäbchenpulver) liegen bei grob Kp 0.025–0.06 / Kd 0.3 und fein Kp 0.8–2.0 / Kd 10–15.
Vihtavuori N140 lag beim User bei grob Kp 0.8. Die Werte werden daher bewusst **niedrig** angesetzt –
die Verdopplungssuche findet größere Werte in wenigen Schritten (0.025 → 0.8 = 5 Verdopplungen).

**Grob-Kp Start**

| | Fine | Medium | Coarse |
|---|---|---|---|
| Ball  | 0.02  | 0.02  | 0.025 |
| Stick | 0.025 | 0.03  | 0.04  |
| Flake | 0.04  | 0.05  | 0.06  |
| Disc  | 0.04  | 0.05  | 0.06  |

**Fein-Kp Start**

| | Fine | Medium | Coarse |
|---|---|---|---|
| Ball  | 0.3 | 0.4 | 0.5 |
| Stick | 0.4 | 0.5 | 0.6 |
| Flake | 0.5 | 0.6 | 0.8 |
| Disc  | 0.5 | 0.6 | 0.8 |

**Kd Start** (für alle Kombinationen): grob 0.1, fein 4.0

Nach ersten realen Tests werden die Werte anhand der CSV-Protokolle angepasst.

## 7. Entscheidungen aus der Review

| Thema | Entscheidung |
|---|---|
| Toleranz Fein | eingestellter `fine_stop_threshold` (nicht fest 0.02) |
| Servo-Gate | falls aktiviert: zu Beginn öffnen und offen lassen |
| Precharge | ignoriert (optionale Hardware) |
| Coarse-Backoff | wie im Charge Mode übernommen |
| Einheit | nur Grain |
| Profil aktivieren | nur nach Bestätigung durch den User |
| LED-Farben | wie im Charge Mode |
| Grob-Kd | Sondierung ab 0.1, Zeitziel 10 s als Grenze, Wiederholung des Startwerts bei Pech, nie 0 (Abschnitt 4.6) |
| Kp-Suchen | Grob: kleinster Kp mit Überwurf **und** Zeitziel; Fein: kleinster Kp, dessen Wurf in 13 s fertig ist (Abschnitt 4.7) |
| Fein-Kd | nicht gesucht, Wert aus der Matrix (D-Anteil pro ms < 1–4 % bei üblichen Werten, verrauscht; Abschnitt 4.8) |
| Verifikation | Runden von 10 Würfen mit festen Werten, verifiziert ab 9/10 Treffern; 2 Überwürfe → Fein-Kp × 0.9 oder Mindestdrehzahl × 0.75, je nachdem was die Drehzahl beim Stopp bestimmt (max. 3 Korrekturen), sonst Ende mit x/10 (Abschnitt 4.9) |
| Stop-Thresholds | werden nicht von der Analyse geändert; der User stellt sie vorher in den Charge-Mode-Einstellungen ein (coarse z. B. 3.0 gr), Anzeige zur Kontrolle in D4. Für `fine_stop_threshold` nur eine Empfehlung aus dem gemessenen Nachfallen: (n + 0.5) × Waagenauflösung, zwischen zwei Waagenstufen, damit der Stopp nicht an der Float-Rundung hängt (Abschnitt 4.10a) |
| Verdopplungsgrenze | jede Suche verdoppelt höchstens 6-mal und halbiert höchstens 5-mal (Abschnitt 4.4) |

## 8. Mögliche Erweiterungen (nicht Version 1)

- **Stopp-Vorhalt pro Profil (Option C, vom User vorgemerkt):** eigener Fein-Stopp-Vorhalt je Profil, getrennt von der globalen Toleranz `fine_stop_threshold`. Erfordert Änderungen an `charge_mode.cpp` und am Profilformat im EEPROM (Revisionswechsel + Umwandlung bestehender Profile). Datengrundlage: gemessenes Nachfallen aus der Analyse (Option B).

- Empfehlung für `coarse_stop_threshold` anhand der gemessenen Grob-Überwürfe (nur Anzeige).
- Precharge / Servo-Gate-Ratio im Testwurf berücksichtigen.

## 9. Code-Struktur (Schritt 4)

### Grundsätze

- Der Code muss **für Menschen gut lesbar** sein: so einfach und verständlich wie möglich, Stil wie die
  vorhandenen Module (`cleanup_mode.cpp`, `charge_mode.cpp`), leicht wartbar. Klarheit vor Raffinesse.
- Es wird **nichts zu GitHub gepusht**. Ein lokaler Commit pro Umsetzungsstufe.

### Neue Dateien

| Datei | Inhalt |
|---|---|
| `src/powder_analysis_config.h` | Alle anpassbaren Werte: Startwert-Matrix, Flut-Drehzahlen, feste Flow-Speeds, Zeitlimits, Obergrenzen, Akzeptanzgrenze 1.0 gr |
| `src/powder_analysis_tuner.cpp/.h` | Reine Suchlogik ohne Hardwarezugriff (Verdopplung, Bisektion, Bewertung, bestes Ergebnis) |
| `src/powder_analysis.cpp/.h` | Zustandsmaschine, Testwurf (Kopie der Charge-Mode-Regelschleife inkl. Backoff, klar kommentiert), Display-Bildschirme, REST-Endpunkte, Speichern |
| `src/html/powder_analysis.html` | Web-Seite (Setup, Status, Diagramm, Protokoll, CSV, Ergebnis) |
| `tests/powder_analysis/` | PC-Unit-Tests der Suchlogik mit simuliertem Pulververhalten |

### Eingriffe in vorhandenen Code (nur Einbindung)

| Datei | Änderung |
|---|---|
| `src/app.h` | `APP_STATE_ENTER_POWDER_ANALYSIS = 11` |
| `src/mui_menu.c` | Hauptmenü-Eintrag „Powder Analysis“ + Startformular |
| `src/menu.c` | `case` im Zustands-Switch + Elternformular für „Back“ |
| `src/rest_endpoints.c` | `#include` + Registrierung von Seite und Endpunkten |
| `src/CMakeLists.txt` | html2header-Regel für `powder_analysis.html` |
| `src/html/web_portal.html` | Link auf die neue Seite |

### Genutzte vorhandene Schnittstellen

- Waage: `scale_block_wait_for_next_measurement()`, `scale_get_current_measurement()`
- Motoren: `motor_set_speed()`, `get_motor_min_speed()`, `get_motor_max_speed()`
- Charge-Mode-Konfiguration (nur lesen): Thresholds, Stabilitätskriterien, Backoff, LED-Farben
- Servo-Gate: `servo_gate_set_ratio()` (falls aktiviert)
- LEDs: `neopixel_led_set_colour()`
- Profile: Slot direkt beschreiben + `profile_data_save()`; `profile_select()` nur nach Bestätigung
- Parallele Bedienung: Muster wie Cleanup/Charge Mode (`exit_state` + `OVERRIDE_FROM_REST`)

### Umsetzungsstufen

1. Konfiguration + Suchlogik + Unit-Tests
2. Ablaufsteuerung, Testwurf, Display-Bildschirme
3. REST-Endpunkte + Web-Seite
4. Einbindung in vorhandenen Code

Nach jeder Stufe: Build, Review gegen dieses Dokument, lokaler Commit.

## 10. Messreihen

Die Rohdaten (CSV-Protokolle) bleiben lokal und werden nicht versioniert (`.git/info/exclude`).
Dateinamen: `docs/<Pulver>_powder_analysis_<Nr>.csv`, z. B. `N135_powder_analysis_1.csv`.

### Messreihe 1 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (19.10.2026)

Thresholds im Gerät: coarse 5 / fine 0.03 (Standard, noch nicht auf 3 / 0.02 umgestellt). 28 Würfe, Ende durch Kd-Limit.

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 4 | Kp 0.06 | 0.0525 → 9.6 s (zu langsam), 0.06 → 1.9 s; Startwert 0.03 passt |
| Grob-Kd | 2 | Kd 0.1 | 1.8 s / 1.9 s → Zeit-Plateau, früher Stopp greift wie gewünscht; User zufrieden |
| Fein-Kp | 6 | Kp 4.0 | 0.5 und 1.0 → Timeout; Startwert 0.5 für Stick/Medium deutlich zu niedrig |
| Fein-Kd | 4 | Kd 8 | |
| Verifikation (alt) | 12 | nicht bestanden | Kd 9.2 → 11.7 ohne Wirkung; Überwürfe +0.04 … +0.13 gr = Nachfallen nach dem Stopp |

Erkenntnisse und Folgen:
- Überwürfe in der Verifikation kommen nicht von Kd, sondern vom Nachfallen → Verifikation testet nur noch, bei ständigem Überwurf wird Fein-Kp gesenkt (Abschnitt 4.9).
- Grobzeiten in vollen Würfen zweigeteilt (1.9 s oder 5.5 s bei gleichen Werten) → Pulver rutscht im Grobrohr ungleichmäßig nach.
- Fein-Zeit ~12 s bei coarse threshold 5 (5 gr Feinanteil); mit 3 gr deutlich kürzer erwartet.
- Fein-Kp-Startwert der Matrix (Stick/Medium 0.5) nach weiteren Messreihen anheben (Messung: 4.0).
- Zielvorgabe User: ein Wurf darf ca. 15 s dauern; wenige Überwürfe sind wichtiger als Tempo.

### Messreihe 2 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (19.10.2026)

Thresholds im Gerät: coarse 3 / fine 0.02. 30 Würfe, **verifiziert 5/5** nach 3 Fein-Kp-Korrekturen.

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 6 | Kp 0.0675, Basiszeit 2.2 s | Jeder Wurf überwirft den Stoppunkt um 0.5–1.2 gr (auch Kp 0.03) → Überwurf-Kriterium immer erfüllt, Entscheidung fällt nur über die Zeit; Zeiten streuen stark (0.075 → 7.3 s, 0.0675 → 2.2 s) |
| Grob-Kd | 1 | Bremse nicht gefunden | Budget 2.2 s × 1.3 = 2.9 s aus einem einzelnen schnellen Wurf; Kd 0.1 → 8.6 s → Abbruch |
| Fein-Kp | 8 | Kp 4.5 | 0.5 / 1.0 → Timeout, 4.0 knapp ohne Überwurf |
| Fein-Kd | 4 | Kd 8 | wie Messreihe 1 |
| Verifikation | 11 | Kp 4.5 → 4.05 → 3.65 → **3.28**, Kd 9.2 | Runden 1–3 je 2 Überwürfe, Runde 4: 5/5, Gesamtzeit 7–12 s |

Erkenntnisse:
- Grobzeiten sind zweigeteilt (≈2 s oder ≈5–8 s bei gleichen Werten). Einzelwurf-Zeitvergleiche (Basiszeit, Bremsbudget) sind dadurch nicht belastbar.
- Grob-Kd wirkt physikalisch kaum (Ableitung pro ms), sein Effekt geht im Zeitrauschen unter.
- Fein-Kp aus der Suche liegt an der Überwurfgrenze; verifiziert wurde erst ~73 % davon.
- Gefundene Werte beider Reihen (Stick/Medium): grob Kp 0.06–0.0675, fein Kp 3.3 (verifiziert), fein Kd 8–9.2.
- Mit coarse threshold 3 liegt die Gesamtzeit bei 7–12 s (Reihe 1 mit 5: ~12 s).

### Messreihe 3 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (19.10.2026)

Neue Startwerte (Medium-Spalte), Thresholds 3 / 0.02. 29 Würfe, **verifiziert 5/5** nach 1 Fein-Kp-Korrektur.

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 5 | Kp 0.075 | Startwert 0.06 nur knapp zu langsam (8.9 s) |
| Grob-Kd | 5 | Kd 0.2 (3.7 s) | alle OK; Zeiten 3.7–8.1 s ohne Trend zu Kd |
| Fein-Kp | 5 | Kp 3.75 | Startwert 3.0 knapp ohne Überwurf |
| Fein-Kd | 5 | Kd 6.0 | |
| Verifikation | 9 | **Kp 3.04 / Kd 6.9** | Runde 1 (Kp 3.375): 2 Überwürfe; Runde 2: 5/5, 6.5–10.7 s |

Gespeichertes Profil im Charge Mode: meist 23.14 gr (0.02 über der Toleranz) – Ursache offen.

### Messreihe 4 – Vihtavuori N560, Stäbchen / grob, Ziel 44.0 gr (21.10.2026)

Thresholds 3 / 0.02. 33 Würfe, **nicht verifiziert** (0 Treffer in 4 Runden).

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 6 | Kp 0.12 | Startwert 0.04 zu niedrig |
| Grob-Kd | 3 | Kd 0.1 | alle OK, 7.6–8.4 s |
| Fein-Kp | 7 | Kp 3.6 | 0.6 / 1.2 → Timeout |
| Fein-Kd | 4 | Kd 3.5 | |
| Verifikation | 13 | – | Kp 3.24 → 2.36: Überwürfe bleiben bei +0.03…+0.06 gr (2–3 Körner), Zeit steigt auf 15–25 s; mehrere 44.000-Würfe nur wegen Zeit kein Treffer |

Beobachtung User: aus dem Fein-Trickler fallen oft 2–3 Körner mit großer Wirkung; die Software stoppt bei Erreichen des Zielgewichts, danach pendelt die Waage höher ein oder ein Korn fällt nach.

Erkenntnisse:
- Im Moment des Fein-Stopps läuft der Fein-Trickler mit Mindestdrehzahl (Kp × ~0.02 gr ≤ 0.06 rps). Das Nachfallen hängt von Mindestdrehzahl, Korngröße und Waagenverzögerung ab, nicht von Kp → Verifikations-Korrektur senkt künftig die Fein-Mindestdrehzahl statt Kp.
- Ein N560-Korn ≈ 0.02 gr = gesamte Toleranz; gelegentliche Überwürfe sind bei grobem Pulver physikalisch kaum vermeidbar (→ Stop-Threshold-Diskussion).
- Coarse-Spalte der Startwert-Matrix angehoben (grob Kp ×3, fein Kp ×5).

### Messreihe 5 – Vihtavuori N560, Stäbchen / grob, Ziel 44.0 gr (21.10.2026)

Thresholds 3 / 0.02, neue Coarse-Startwerte, Verifikations-Korrektur über Mindestdrehzahl. 27 Würfe, nicht verifiziert (Runde 2: 2 Treffer).

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 4 | Kp 0.12 | Startwert passt (sofort OVER) |
| Grob-Kd | 2 | Kd 0.1 | OK, 9.2–9.5 s |
| Fein-Kp | 5 | Kp 3.75 | |
| Fein-Kd | 8 | Kd 128 (!) | Gewichte bei Kd 8…112 ohne Trend (+0.016…+0.092), Treffer bei 128 Zufall; Würfe 14–18 s |
| Verifikation | 8 | Runde 2 (Min 0.045): 2/5 | Runde 1 (0.06): 2 Überwürfe nach 3 Würfen; Runde 2: 1 Überwurf, 2 Würfe nur wegen Zeit kein Treffer |

Beobachtung User: N560-Stäbchen sind unterschiedlich lang → unterschiedliches Korngewicht (≈ 0.02–0.04 gr).

Erkenntnisse und Folgen:
- Bei ±0.02 gr entscheidet ein einzelnes Korn; Einzelwurf-Bewertungen sind zufällig → für grobes Pulver `fine_stop_threshold` 0.04 (vom User global einzustellen, beim Pulverwechsel zurückstellen).
- Mindestdrehzahl-Korrektur wirkt (Runde 2 deutlich weniger Überwürfe).
- Fein-Kd-Suche auf 3 Verdopplungen begrenzt; ohne Treffer wird der Startwert genommen (Kp bleibt).

### Messreihe 6 – Vihtavuori N560, Stäbchen / grob, Ziel 44.0 gr (21.10.2026)

Thresholds 3 / **0.04**, Fein-Kd-Suche auf 3 Verdopplungen begrenzt. 27 Würfe, Verifikation 4/5 (Runde 2, Min 0.045), Würfe 9.6–13.7 s.

Vergleich der drei N560-Reihen:

| | Reihe 4 (fs 0.02) | Reihe 5 (fs 0.02) | Reihe 6 (fs 0.04) |
|---|---|---|---|
| Grob-Kp | 0.12 | 0.12 | 0.15 (0.135 knapp zu langsam) |
| Grob-Kd | 0.1 | 0.1 | 0.1 (alle 4 Stufen OK, 7.0–8.3 s) |
| Fein-Kp gefunden | 3.6 | 3.75 | 5.25 |
| Fein-Kd gefunden | 3.5 | 128 (Rauschen) | 16 |
| Verifikation | 0 Treffer | 2/5 | 4/5 |
| Verifikationswürfe innerhalb ±0.04 | – | 6/8 | 6/9 |
| Mittlere Abweichung Verifikation | +0.04 | +0.03 | +0.03 |

Erkenntnisse:
- Grob-Werte reproduzierbar (Kp 0.12–0.15, Kd 0.1).
- Fein-Kp steigt mit größerem Threshold (früherer Stopp → mehr Kp nötig für Überwurf); innerhalb gleicher Thresholds konsistent (3.6 / 3.75).
- Fein-Kd bei grobem Pulver nicht reproduzierbar → Wert wird vom Rauschen bestimmt.
- Trefferquote bei ±0.04 in Reihe 5 und 6 gleich (~70 %), Ergebnisse liegen systematisch ~+0.03 gr hoch → physikalische Grenze durch Nachfallen (~2–3 Körner nach dem Stopp), kein Zufall.

### Messreihe 7 – Vihtavuori N320, Stäbchen / fein, Ziel ~8 gr (22.10.2026)

26 Würfe, nicht verifiziert. **Durch einen Programmfehler verfälscht, nicht für Startwerte verwenden.**

- 13 von 26 Würfen enden nach genau 1.6 s mit 12–13 gr; der User sah den Trickler dabei deutlich schneller drehen.
- Ursache: Division durch null im D-Anteil der Regelschleife. Liegt beim Wurfstart schon ein Messwert bereit, ist der Abstand zur ersten Messung 0 ms → Ableitung unendlich/NaN → beide Trickler laufen bis zum nächsten Messwert mit Höchstdrehzahl.
- Der Fehler steckt im Original (`charge_mode_wait_for_complete()`), die Analyse hat ihn mitkopiert. Behoben in der Analyse; im Charge Mode bewusst zurückgestellt (Entscheidung User).
- Rückblickend vermutlich auch Ursache der zweigeteilten Grobzeiten in den N135- und N560-Reihen (bisher als ungleichmäßiges Nachrutschen gedeutet).

### Messreihe 8 – Vihtavuori N320, Stäbchen / fein, Ziel 8.2 gr (22.10.2026)

Erste Reihe mit behobenem D-Fehler; Thresholds 3 / 0.02 (laut User; aus den Bewertungen nicht eindeutig ableitbar). 29 Würfe, nicht verifiziert (1 Treffer).

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 7 | Kp 0.1125 | Grobzeiten jetzt gleichmäßig (5.8–7.3 s), keine 1.6-s-Ausreißer mehr |
| Grob-Kd | 2 | Kd 0.1 | OK, 7.2 s |
| Fein-Kp | 7 | Kp 5.6 | bester Wurf Kp 4.8: 8.236 gr in 8.1 s (ohne Überwurf) |
| Fein-Kd | 4 | Startwert (keine Wirkung) | Kd 4/8/16/32 → 8.29 / 8.30 / 8.29 / 8.29 gr |
| Verifikation | 9 | – | Kp 5.04: 8.25–8.30 gr; Mindestdrehzahl 0.06 → 0.025 ohne Wirkung |

Beobachtung User: der Fein-Trickler bremst am Schluss nicht genug ab.

Erkenntnisse:
- Drehzahl beim Stopp = max(Kp × Abweichung, Mindestdrehzahl). Bei N320 (Kp 5, Threshold 0.02) ≈ 0.1 rps > Mindestdrehzahl → Kp bestimmt das Nachfallen, die Mindestdrehzahl-Korrektur verpufft.
- Die Fein-Kp-Suche wählt bewusst den kleinsten überwerfenden Kp (Tuning Guide); Kd bremst in dieser Firmware aber praktisch nicht → systematischer Überwurf.
- Bevor die Regeln geändert werden, wird die Wirkung von Fein-Kd mit dem Testwurf mit Aufzeichnung gemessen (Abschnitt 5.3).

### Messung 9 – Wirkung von Fein-Kd (Testwurf mit Aufzeichnung, N320, 22.10.2026)

6 Testwürfe, Ziel 8.2 gr, grob Kp 0.11 / Kd 0.1, fein Kp 4.8, Mindestdrehzahl 0.06, Threshold 0.02; nur Fein-Kd variiert. Hinweis: die Aufzeichnung enthielt den Stopp-Messwert noch nicht (letzter Wert 8.16 = vorletzter Messwert); wird nachgerüstet. Waage liefert ~alle 50 ms einen Wert, ändert ihn aber nur jedes zweite Mal (Schritte 0.02 gr).

| Fein-Kd | D-Anteil kurz vor dem Stopp | Anteil an der Fein-Drehzahl |
|---|---|---|
| 0 / 0.2 / 0.8 | 0 … −0.0003 rps | < 0.2 % |
| 4 | −0.0013 … −0.002 rps | < 1 % |
| 16 | −0.005 … −0.008 rps | 2–4 % |
| 64 | −0.02 … −0.03 rps | 10–17 %, springt zwischen 0 und Wert |

Letzte aufgezeichnete Fein-Drehzahl in allen Würfen 0.19 rps (Kp 4.8 × 0.04 Restabweichung); beim Stopp selbst ≈ Kp 4.8 × 0.02 ≈ 0.1 rps. Das Bremsen am Schluss übernimmt allein der P-Anteil.

Folgen: Fein-Kd wird nicht mehr gesucht (Startwert aus der Matrix); Fein-Kp-Suche nimmt den größten Kp ohne Überwurf; die Verifikations-Korrektur senkt Kp oder Mindestdrehzahl, je nachdem was beim Stopp die Drehzahl bestimmt; Fine-Spalte der Startwerte angehoben.

### Messreihe 10 – Vihtavuori N320, Stäbchen / fein, Ziel 8.2 gr (22.10.2026)

Neue Fein-Logik (größter Kp ohne Überwurf, Kd fest 4.0, Korrektur Kp oder Mindestdrehzahl), Thresholds 3 / 0.02. **18 Würfe** (Reihe 8: 29), Verifikation 4/5 (Runde 2), Würfe 8.4–9.5 s.

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 4 | Kp 0.10 | Startwert 0.10 sofort passend |
| Grob-Kd | 2 | Kd 0.1 | OK, 6.9 s |
| Fein-Kp | 5 | Kp 4.5 | Startwert 4.0 Treffer; 5.0 überwirft |
| Verifikation | 7 | Kp 4.05 / Kd 4.0, 4/5 | Runde 1 (Kp 4.5): 2 Überwürfe → Kp -10 %; Runde 2: 1 Überwurf |

Aufzeichnung der Verifikationswürfe (mit Stopp-Messwert): alle Würfe stoppen bei 8.18 gr; die Drehzahl davor ist Kp × 0.04 (0.16–0.18 rps), weil die Waage in 0.02-gr-Schritten und nur jeden zweiten Messwert aktualisiert. Nachfallen nach dem Stopp (Endgewicht − Stoppgewicht, aus Protokoll zugeordnet): +0.008 … +0.056 gr, meist +0.016; Überwürfe entstehen durch gelegentlich größeres Nachfallen (+0.05). Endgewicht fehlte in der Aufzeichnung (Fehler, behoben).

### Messreihe 11 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (22.10.2026)

Firmware mit allen Korrekturen (D-Fehler behoben, neue Fein-Logik), Thresholds 3 / 0.02. 22 Würfe, **nicht verifiziert**. Charge Mode mit dem Ergebnis: 4 von 5 Würfen Überwurf (der eine Treffer durch ein verklemmtes Korn).

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 5 | Kp 0.0825 | Grobzeiten gleichmäßig (5.4–8.9 s) |
| Grob-Kd | 2 | Kd 0.1 | OK |
| Fein-Kp | 4 | Kp 4.5 | |
| Verifikation | 11 | Kp 4.5 → 4.05 → 3.65 → 3.28 | fast nur Überwürfe +0.008 … +0.068 |

**Ursache:** Alle aufgezeichneten Würfe stoppten erst bei 23.100 gr (= Ziel), nicht bei 23.08. Stopp-Bedingung `Abweichung < Threshold` in float: 23.1 − 23.08 = 0.0200005 → nicht < 0.02 → Stopp erst am Ziel; jedes nachfallende Korn ist ein Überwurf. Bei 8.2 gr (N320) ergibt dieselbe Rechnung 0.0199995 → Stopp bei 8.18. Mit einem Threshold genau auf einem Waagenschritt (0.02) entscheidet also die Rundung, ob einen Schritt früher oder am Ziel gestoppt wird – im Charge Mode genauso.

Nachfallen nach dem Stopp: Median 0.028 gr (0.016 … 0.068).

Beobachtung User: die Waage zeigt nach dem Wurf oft mehr an als das Gerät bewertet hat. Erklärung: Stopp am Ziel + Nachfallen; zusätzlich bewertet der Charge Mode den Wurf bereits 1 s nach dem Stopp, ohne auf ein stabiles Gewicht zu warten.

Folgen: Fein-Threshold 0.03 (zwischen zwei Waagenschritten) → Stopp zuverlässig bei Ziel − 0.02, gleiche akzeptierte Anzeigewerte; die Empfehlung der Analyse schlägt künftig immer Werte zwischen zwei Schritten vor. Optional später: Stopp-Bedingung robust gegen Rundung machen (Charge Mode + Analyse).

### Messreihe 12 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (22.10.2026)

Fein-Threshold **0.03**. 17 Würfe, **verifiziert 5/5** (Kp 3.0 / Kd 8.0, 7.9–13.3 s). Stopp jetzt zuverlässig bei 23.08; Nachfallen in der Verifikation 0 … +0.028.

Charge Mode mit diesem Profil (13 Würfe): 23.18, 23.08, 23.10, 23.10, 23.12, 23.12, 23.10, 23.16, 23.14, 23.10, 23.08, 23.10, 23.14 → 9/13 innerhalb ±0.03; Nachfallen meist +0.02, selten +0.06 … +0.10. Wurfzeiten 7–8.9 s.

Nachfallen in Abhängigkeit von Fein-Kp (gleiche Reihe): 6.0 → +0.156, 4.5 → +0.056, 3.75 → +0.080, 3.375 → +0.052, 3.0 → 0 … +0.028 → je langsamer der Trickler beim Stopp, desto weniger fällt nach.

Folgen: Fein-Kp = kleinster Kp, der innerhalb von 13 s fertig wird (Zeitbudget für Präzision nutzen); Verifikation 10 Würfe, verifiziert ab 9 Treffern (5 Würfe erfassten die seltenen großen Nachfaller nicht).

### Messreihe 13 – Vihtavuori N135, Stäbchen / mittel, Ziel 23.1 gr (23.10.2026)

Firmware nach Review/Aufräumen; Fein-Threshold 0.03; Fein-Kp = kleinster Kp innerhalb 13 s; Verifikation 10 Würfe. **22 Würfe, verifiziert 9/10** ohne Korrektur.

| Phase | Würfe | Ergebnis | Beobachtung |
|---|---|---|---|
| Grob-Kp | 5 | Kp 0.075 | wie Reihe 12 |
| Grob-Kd | 2 | Kd 0.1 | OK, 6.9–7.0 s |
| Fein-Kp | 5 | Kp 2.06 | 3.0 → 6.7 s, 1.5 → 14.9 s (zu langsam), Bisektion → 2.06 (11.2 s) |
| Verifikation | 10 | 9/10 | Wurfzeiten 9.4–11.7 s; Nachfallen meist 0 … +0.012, ein Ausreißer +0.052 |

Drehzahl vor dem Stopp 0.083 rps (Reihe 12 mit Kp 3.0: 0.12 rps). Stopp zuverlässig bei 23.08.

Charge Mode mit diesem Profil (11 Würfe): 23.12, 23.08, 23.12, 23.08, 23.08, 23.08, 23.08, 23.12, 23.08, 23.08, 23.08 → **11/11 innerhalb ±0.03**, Wurfzeiten 10.0–14.0 s. Die Würfe landen überwiegend an der unteren Toleranzgrenze (Stopp bei Ziel − 0.02, kaum Nachfallen) – Datengrundlage für den vorgemerkten Stopp-Vorhalt pro Profil (Abschnitt 8).

Ergebnis: Zeitbudget-Regel, Threshold zwischen zwei Waagenschritten und 10-Wurf-Verifikation wirken; N135 bestätigt.
