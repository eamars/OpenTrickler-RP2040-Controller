---
name: web-ui-design
description: Design rules for this firmware's web pages (ADR-008), served by the unit to a phone, a tablet or a computer at a reloading bench in a garage. Drawn from touch and pointer research (Hoober's observations of phone grips and touch accuracy, Apple's and Material's target sizes, WCAG 2.2), Material's window size classes, industrial operator-screen practice (ISA-101's high-performance HMI), NN/g on confirmations, and captive-portal browsers' limits. Use before proposing, building or reviewing any page, layout, control, colour, icon or wording of the web UI; when a page looks "plain", "busy", "small" or "hard to use with gloves"; when choosing what goes on a phone, a tablet or a computer; and before adding a dialog, an animation or anything loaded from the internet. The mock comes first, on tests/web_test.py.
---

# Web UI design (phone, tablet, computer; touch and pointer)

**The device and where it is used:**
- **The pages** come from the unit itself, over plain HTTP, from a small server (six connections at once, lwIP's 40 KB heap; ADR-003). Each page is one file, its styles and script inside it, gzipped (ADR-008). A page served this way gets no Screen Wake Lock, no service worker and no WebCrypto: browsers give those only to HTTPS and localhost.
- **The phone:** propped beside the trickler, read at arm's length (50 to 70 cm) between other jobs, touched with a finger that may be dirty or gloved. Held in one hand when walked up to. Portrait.
- **The tablet:** on a stand at the bench, portrait or landscape, touched; read from further than a phone.
- **The computer:** at a desk, a mouse and a keyboard, a wide window; setting up, powders side by side, the update, backups.
- **The light:** from a bright garage door to a dim shed, so the pages follow the device's light or dark setting and keep their contrast in both.
- **The set-up's first page** may open in a phone's captive-portal sheet: a reduced browser (on iOS, no persistent cookies and no storage, by vendor reports), closed when the person leaves it (ADR-008, "Commissioning").
- **The screen at the unit** shows the same state in its own way (`embedded-ui-design`); the web uses the screen's words (ADR-008, "The words").

## Rules

1. **Lay out by the window's width, not by the device.**
   - Three widths, Material's window size classes: compact under 600 CSS px (a phone), medium 600 to 839 (a tablet in portrait, a phone in landscape), expanded 840 and over (a tablet in landscape, a computer).
   - One page that grows with the width, never a second page for phones. Compact: one column. Medium: one column, wider, the figure larger. Expanded: the same one column, centred, no wider than about 640 px; a job's page never puts its groups side by side (the owner, 2026-11-18: "I think the side-by-side design is just wrong", after a panel beside its controls first left the controls hanging at the bottom right and then put the target right of the weight). A wide window gives the column more height and a larger figure, not a second column. Lists of settings may take two columns.
   - **Navigation:** a bar at the bottom on compact, with its four destinations each a whole quarter of the width (the thumb's reach); a rail down the left from medium up. Four destinations: Trickler, Powders, Settings, Unit (the owner, 2026-11-18: the Trickler holds both jobs, Charge and Purge, by a switch at its top).
   - **Why:** Material 3 sets these widths and moves the navigation bar to a rail on larger windows; web.dev advises against guessing the input from the screen's size (rule 3 does that).
2. **Targets large enough for a gloved finger.**
   - Every control at least 48 by 48 CSS px, with at least 8 px between neighbours. That is Material's 48 dp (about 9 mm), above Apple's 44 pt and WCAG's 44 px (2.5.5, AAA); WCAG's 24 px (2.5.8, AA) is a floor, never a target.
   - The controls used during a job (Start, Stop, the purge's speed) at least 64 px tall and the column's whole width.
   - Nothing that acts in a corner; the main action in the lower middle, where a thumb reaches. People touch the middle of a screen more accurately than its edges: about 7 mm suffices there, about 12 mm in the corners (Hoober).
   - Gloves: no standard was found. Vendors suggest 20 mm and more for work gloves (not checked against a primary source). Test with the owner's gloves on a real phone before trusting a size.
3. **Touch first; the mouse and the keyboard as extras.**
   - Nothing depends on hovering: no tooltip that holds information, no menu that opens on hover. Hover may only add a highlight, inside `@media (hover: hover) and (pointer: fine)`.
   - Size by `pointer`, not by width: under `(pointer: coarse)` controls stay large; a fine pointer may make lists denser, never the job's controls.
   - Every control reached by Tab, its focus drawn plainly (3 px outline), Enter and Space acting as a click. On a computer, the target's field takes the arrow keys and Enter starts.
4. **The figure, read at a glance.**
   - One figure a job page is about (the weight during a charge, the speed during a purge), as on the screen. Its capitals at least 8 mm on a phone (about 20 arcminutes at 1.4 m, the display standards' optimum; `embedded-ui-design`, rule 3); about 72 CSS px a glyph on a phone, more on a tablet.
   - Tabular figures, a fixed number of decimals (the unit's `decimal_places`): the digits do not jump as the weight moves.
   - Its unit small beside or below it; its context (the target, the verdict) on one line under it.
   - **Why:** a reading at a glance from the length of an arm, between other jobs.
5. **Grey for normal; colour only where it means something.**
   - The page is calm neutral greys. Colour is kept for state a person must notice and for the one action that moves the job on.
   - **State colours, the unit's own meanings** (its lights, `settings/charge`): blue not ready, green good, yellow under, red over and faults. Toned for a screen (the lights' pure `#00ff00` glares); never colour alone: each state also has its word.
   - **A normal charge is not an alarm:** it shows progress in the neutral ink, not in a warning colour. Under, over and a fault stand out because nothing else does.
   - **Why:** ISA-101's high-performance HMI keeps screens grey when normal and saves bright colour for the abnormal, so that it is seen (its authors: grey is the common way, not a requirement).
6. **Contrast holds in any light.**
   - Text 4.5:1 against its ground, large text (the figure, headings) 3:1 at the least; a control's edge, an icon and a state mark 3:1 against what is around them (WCAG 1.4.3, 1.4.11). Aim higher for the figure: it is read in glare.
   - **Dark:** dark grey grounds, not pure black; colours lighter and less saturated than in light, so they keep their contrast (Material's dark theme). Secondary text no fainter than about 70%.
   - Check both themes with the browser's colour-scheme emulation before showing a page.
7. **Forms.**
   - Labels above their fields; the unit beside the value; a field's help line under it from the schema's `about`.
   - Every field's text at least 16 px: iOS Safari zooms into a smaller field and leaves the page zoomed. Never stop the person zooming (`maximum-scale`).
   - Numbers with `inputmode="decimal"`; the step and the limits from the schema; a refusal said beside the field, in the unit's words.
   - Choices of a few options as segmented buttons; of many, a list. Never a Yes/No select for an action: an action is a button (today's System Control, `features.md` rows 70 and 74).
   - A setting is live when written, and the page shows whether it is stored; storing is one plain action, not a question at every change (to settle with the mock, ADR-008, "The words").
8. **Confirm only what cannot be undone; ask for safety.**
   - Routine actions act at once: a change of target, of powder, a zero, a stop.
   - **Irreversible actions** (a factory reset, an update's install) ask in a dialog that says what is lost, with buttons named by their outcome ("Erase every setting", "Keep my settings"), no default, the safe one on the left. Not more often than that, or the dialog is clicked through unread (NN/g).
   - **The cup** before a charge and a purge is a safety step, not a confirmation (the owner, 2026-11-17): short, the screen's words ("Put the empty cup on the scale", "To catch the powder"), its yes named by what is true ("The cup is on").
9. **The unit's state always in view.**
   - Whether the unit is reached: said only when it is not, in a banner over the page ("Unit not reached. Trying again..."), gone when it answers. A permanent row with "Connected" and the unit's name was a waste of space (the owner, 2026-11-18); normal takes no room (rule 5). The unit's name is on the Unit page.
   - The job that runs, wherever the person is: a page that is not the job's shows a strip with the job and its Stop.
   - What the unit waits for, in words (the screen's: "Lift the cup", "Return the cup").
   - Every client echoes the unit (ADR-008): the page shows what the unit says, not what it last sent.
10. **Motion only where it means something.**
    - A progress ring or bar may ease between readings (under 300 ms); the figure itself never animates.
    - Nothing moves for decoration. Under `prefers-reduced-motion: reduce`, nothing eases.
11. **Small, and nothing from outside for the basics.**
    - The system's own fonts (no web font), icons drawn inline in SVG, no image files, no framework at run time. The whole UI about 50 KB gzipped (ADR-008, "The budget").
    - An advanced page may load a pinned library from the internet, with its integrity hash, and says so when there is none (ADR-008, "Resources from the internet").
12. **Words.**
    - The screen's words, one list (ADR-008, "The words"): Trickler (the page), Charge, Purge, Powder, Zero, Motors, Servo gate, Factory reset, Restart, Update, Hotspot. The motors are never "tricklers".
    - What the person does is an imperative ("Lift the cup"); what the unit does is a state ("Charging", "Zeroing"): the screen's job titles, word for word (`charge_state_text()`, `menu_nav.c`). Buttons are verbs that say what happens ("Start charge", "Stop purge").
    - One language, English. Units as the screen writes them (gr, g, rev/s).
13. **The set-up works in a captive-portal sheet.**
    - One column about 480 px wide, everything in the page, no storage relied on, a whole-page load (not only a script's update) when a step needs the phone to look at the network again.
    - It must also work in a full browser reached by the unit's address.

14. **A job's page fits the screen: it never scrolls.**
    - The Trickler's views (Charge, Purge) fit the visible height, down to a phone of 375 by 667 CSS px with its browser's bars shown (about 550 px for the page). Sized by `svh`, the height with the bars shown, never `vh`, which is taller than what a phone shows.
    - What does not fit is scaled, not scrolled: the figure's area takes the room left (a CSS container), and the figure and its ring scale with it; where the room is too short for a ring, a bar under the figure takes its place, and the steps give way to the status word, which says the same. The figure never goes below rule 4's size: the ring and the steps go first.
    - Only what a job needs while it runs is on its view. A preference set once (keeping the phone awake) goes to the Unit's page.
    - Lists (powders, settings, the update's notes) may scroll: they are read, not worked at.
    - **Why:** the owner, 2026-11-18: "scrolling a control page seems inconvinent", which is why the charge and the purge were two pages before.

15. **Group by what each element is about, then lay out the groups.**
    - Before placing anything, sort the elements into groups by what they act on or describe. On the charge: **the charge** (what to throw: the powder, the target), **the scale** (what is on the pan: the weight, its progress to the target, Zero), **the job** (where the cycle is and what to do: the status, the steps, Start or Stop). A parameter goes with its sibling parameters, an action with the thing it acts on (Zero with the weight, not with the powder).
    - The groups always read in the same order, top to bottom or left to right: inputs, then the measurement, then the action. Never an input to the right of, or under, the reading it sets (the target is never right of the weight).
    - The groups stack in that order at every width (rule 1): never side by side.
    - **Why:** the owner, 2026-11-18: "I don't think in any case the charge weight should be put on the right hand side of the measurement. Check your web element inclusive relationship." The target had been put with Start, and Zero with the powder; on a wide window the target then sat right of the weight.
    - On the tiniest phones the page should look right before it is easy to reach: scale down, do not rearrange (the owner, the same day).

## This project's design language

- **Ground and surfaces:** a neutral grey ground, cards a step lighter (light) or darker (dark), 1 px rules, corners of 14 to 18 px, little shadow.
- **The job's panel:** the figure, its ring or bar, the status word and the steps in one card; the controls in the card beside or below it.
- **Accent:** one colour for the action that moves the job on (Start), red for Stop; state colours as rule 5.
- **Faces:** the system's sans serif; figures tabular, bold; three sizes besides the figure (body 17 px, small 14 px, heading 20 px).
- **Icons:** 24 px, a 2 px stroke, round ends, `currentColor`; one per navigation destination and per card title.

## Process

- **Design in the real pages, on the stand-in.** The sources are `src/web/`, built by `scripts/web_build.py` into `build_web/`; `tests/web_test.py` serves them at `/mock/` against a simulated unit. The mock is the final pages, not a drawing of them (ADR-008).
- **Look at every page at three widths** (375, 768 and 1280 px) **and in both themes** before the owner sees it: screenshots in the agent's browser, by eye. Overlaps, crowding and faint text show only in the picture.
- **Drive it:** start a charge, a purge, a refusal, the unit not reached; check the browser's console for errors.
- **The owner decides by using it,** on their own phone, tablet and computer: send the address, not a description.
- **Every feature carries its row** of `docs/developments_plans/ADR-008-web_ui/features.md`: nothing silently dropped.

## Relation to the other project skills

- `embedded-ui-design` decides the 12864's screens; this skill the web's. Where they share a thing (a word, a state's meaning, the cup's step) they say the same; when they differ, the ADR settles it and both are changed.
- `py-style` for `scripts/web_build.py` and `tests/web_test.py`.

## Sources

- **Target sizes:** W3C, Understanding SC 2.5.8 Target Size (Minimum) (24 by 24 CSS px; 2.5.5's 44 by 44 for important controls), https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html. Apple's 44 pt and Material's 48 dp as reported by many secondary sources (the platforms' own pages were not read here).
- **Grips and touch accuracy:** Steven Hoober, "How Do Users Really Hold Mobile Devices?", UXmatters, 2013 (780 touches: 49% one hand, 36% cradled, 15% two hands; the author's later note: the middle is touched more often, faster and more accurately than the corners), https://www.uxmatters.com/mt/archives/2013/02/how-do-users-really-hold-mobile-devices.php; and "Design for Fingers, Touch, and People, Part 1", UXmatters, 2017 (targets of 7 mm in the middle, about 12 mm in the corners), https://www.uxmatters.com/mt/archives/2017/03/design-for-fingers-touch-and-people-part-1.php
- **Window size classes:** Material 3 (compact under 600 dp, medium to 840, expanded from 840; a navigation rail from medium), as given by Android's developer guides and several implementations; m3.material.io's page was not reached, https://developer.android.com/develop/ui/compose/layouts/adaptive/window-size-classes
- **Input by media features:** web.dev, "Interaction" (use `pointer` and `hover`, not the screen's size), https://web.dev/learn/design/interaction; MDN, `pointer` and `any-pointer`, https://developer.mozilla.org/docs/Web/CSS/@media/pointer
- **Grey screens, colour for the abnormal:** ISA, "The High Performance HMI", https://blog.isa.org/the-high-performance-hmi; Control.com, "Going Gray: A New HMI Standard" (grey is popular, not required), https://control.com/technical-articles/going-gray/. ISA-101.01 itself was not read.
- **Contrast:** W3C, Understanding SC 1.4.11 Non-text Contrast (3:1 for controls, states and icons), https://w3c.github.io/wcag21/understanding/non-text-contrast; 1.4.3's 4.5:1 for text.
- **Dark theme:** Material Design 2, "Dark theme" (dark grey, desaturated colours to keep AA contrast), https://m2.material.io/design/color/dark-theme
- **Confirmations:** NN/g, "Confirmation Dialogs Can Prevent User Errors (If Not Overused)" (only for serious, irreversible actions; buttons named by outcome; no default; undo where possible), https://www.nngroup.com/articles/confirmation-dialog/
- **iOS field zoom:** CSS-Tricks, "16px or Larger Text Prevents iOS Form Zoom", https://css-tricks.com/16px-or-larger-text-prevents-ios-form-zoom/
- **Captive-portal browsers:** Wireless Broadband Alliance, captive portal behaviour by platform, https://captivebehavior.wballiance.com/; vendor guides report no storage and no persistent cookies in iOS's sheet (not checked on a phone: ADR-008 defers the iPhone).
- **Size by visual angle:** Activu, "Optimal Font Size" (16 arcminutes minimum, 20 optimal), https://activu.com/uploads/Optimal-Font-Size.pdf
- **Gloves:** no primary source found; vendor articles (CDTech, AZ Big Media) suggest 20 mm and up. To be measured with the owner's gloves.
