# The screen's fonts

The menu's faces, as BDF bitmap fonts (ADR-010, gate 4). `scripts/bdf2lvgl.py` turns them into LVGL fonts at build time, and `tools/ui_mock` draws the mock-ups with them, so a mock-up shows what the unit draws.

| File | Used for |
|---|---|
| `helvR08.bdf` | Text, and the title marks' icons (`bdf2lvgl.py` adds them) |
| `helvB08.bdf` | Titles |
| `helvB18.bdf` | A job screen's figure |
| `helvR14.bdf` | The mock-up sheet's captions only; not in the firmware |

They are Adobe's and Digital's X11 Helvetica, with Markus Kuhn's ISO 10646 extension, as u8g2 carries them (`tools/font/bdf`, at its commit 2b75f932, 2025-02-22). Copied unchanged. A face tried for a mock-up is added here as its BDF file.

## Their notice

Every file carries it in its `COMMENT` lines. The firmware draws with these fonts, so it is given here as well:

> Copyright 1984-1989, 1994 Adobe Systems Incorporated.
> Copyright 1988, 1994 Digital Equipment Corporation.
>
> Adobe is a trademark of Adobe Systems Incorporated which may be registered in certain jurisdictions. Permission to use these trademarks is hereby granted only in association with the images described in this file.
>
> Permission to use, copy, modify, distribute and sell this software and its documentation for any purpose and without fee is hereby granted, provided that the above copyright notices appear in all copies and that both those copyright notices and this permission notice appear in supporting documentation, and that the names of Adobe Systems and Digital Equipment Corporation not be used in advertising or publicity pertaining to distribution of the software without specific, written prior permission. Adobe Systems and Digital Equipment Corporation make no representations about the suitability of this software for any purpose. It is provided "as is" without express or implied warranty.
