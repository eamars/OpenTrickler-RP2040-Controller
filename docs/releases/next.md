# For the next release's notes

What a change since the last release asks its notes to say, written when the change is made. Step 1 of a release (README) carries each item into its notes and empties this list.

- **Each unit's name changes once** (ADR-008, gate 4, step 1). Its id is made from its Pico's own permanent id now, not drawn at random and stored: its host name (`opentrickler-XXXX.local`) and its hotspot's name change with this update, and stay the same from then on, across a factory reset too. A bookmark of the old name finds nothing. A unit given another Pico takes that Pico's name.
- **A router's list of clients names each unit** as `opentrickler-XXXX` (the same step), where every unit was `PicoW`: a unit's address can be found there by its name.
- **Each unit's hotspot has a key of its own** (the same step), shown on its screen (Wi-Fi, on the main menu, while it is its own hotspot). The key `opentrickler` every unit shared no longer joins.
