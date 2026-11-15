# ADR-002: the notes and comments, as drafts

Each is shown to the owner before it is posted, and none is posted without the owner's word (README, step 2). They are written to be posted from the owner's account. The links point to main, so they work once this ADR's branch is merged.

Status of each: **draft**, until the owner has read it. `progress.md` records what was posted and when.

## PR #113, a comment (the PR stays open)

> Thank you for this, and sorry that it sat for a month without an answer. It is a large and careful piece of work. The tuner with its tests on a PC and the simulated powder stand out.
>
> Where it stands:
>
> **Taken now.** Your fix for the D term in `charge_mode.cpp` (no D term on a throw's first reading, nor when no time has passed) goes into main on its own, in a commit that credits you, once we have shown the fault on a unit.
>
> **Not merged as it stands,** for two reasons:
> - The analysis runs its own copy of charge mode's loop. Since you opened the PR, charge mode's loop has learned to pause the tricklers when the scale's reading goes missing and to end a charge when a motor driver reports a fault. A copy does not have that, and we want one loop to move the tricklers.
> - The roadmap rebuilds what the screens and the page stand on: the display moves to LVGL, and the web server and its pages are replaced. Screens and a page merged now would be built a second time.
>
> **What happens to it.** The roadmap has a step for exactly this subject: the control loop as a module tested on a PC, every throw recorded, and automatic tuning on top ([ADR-009](https://github.com/eamars/OpenTrickler-RP2040-Controller/blob/main/docs/developments_plans/ADR-009-controller_and_tuning/README.md)). Your tuner, its tests and the simulated powder are written into it as candidates. When that step is designed they are judged against the design, and what fits goes in with your commits or with credit to you. This PR stays open until then.
>
> To be straight about the time: several steps come before that one, so it will not be soon. If you would like to say how you would cut the tuner from the loop, or what you would do differently now, this is a good place for it.

## PR #111, a comment (the PR stays open)

> Thank you for this, and sorry for the silence since July.
>
> Update over Wi-Fi is on the roadmap as its own step ([ADR-004](https://github.com/eamars/OpenTrickler-RP2040-Controller/blob/main/docs/developments_plans/ADR-004-update_over_wifi/README.md)), and your design is written into it as its first view: the small bootloader, the staging area, the checks as the image is written, and your lesson that the running image never writes its own region.
>
> It is not merged before that step, for one reason above the others: a new flash layout reaches a user's unit only by cable, and every later change to the layout costs every user another update by cable. So we want to decide the layout once, for the Pico W and the Pico 2 W together. The upload also sits on the web server that is replaced in the step before.
>
> This PR stays open and is decided there. Did the Pico 2 W arrive, and did you find why its build fails in CI?

## PR #109, a closing note

> Thank you for this, and for #107, #108 and #110. The simulated scale from #107 is now part of every proof we run on a unit.
>
> This one is not merged, and the reason is where the weight is kept. The plan is a charge profile beside the powder profile: a name, a weight and the powder it uses, so that two calibres on one powder share that powder's tuning. With the weight inside the powder profile, a second calibre needs a second copy of the powder. It would also change the stored profiles now and again when charge profiles come.
>
> The need you built this for, not dialling the weight in at every start, is written into that plan ([ADR-014](https://github.com/eamars/OpenTrickler-RP2040-Controller/blob/main/docs/developments_plans/ADR-014-charge_profiles/README.md)). Your view on what a charge profile should hold is welcome there.

## PR #101, a closing note

It is the owner's own PR, opened to compare Jump73's port. Whether Jump73 is named with an `@`, and so notified, is the owner's choice.

> Closing this. Automatic tuning is planned as its own step ([ADR-009](https://github.com/eamars/OpenTrickler-RP2040-Controller/blob/main/docs/developments_plans/ADR-009-controller_and_tuning/README.md)), built on one control loop that is tested on a PC. The ideas of this port are written into it: tuning the coarse trickler alone and then the fine one, the search that halves its step, new gains suggested from ordinary charges, and the allowance for a scale that reports late. Thank you, Jump73, for the work behind it.

## PR #28, a closing note

> Thank you for this, and sorry it stayed open so long. Update over Wi-Fi is planned as its own step ([ADR-004](https://github.com/eamars/OpenTrickler-RP2040-Controller/blob/main/docs/developments_plans/ADR-004-update_over_wifi/README.md)), with the upload in the browser and a small bootloader. The bootloader here carries a Wi-Fi stack of its own, about 400 KB, which the Pico W cannot spare. What this PR showed is written into that plan.

## PR #44 and PR #46, the owner's own

No note. #44 is closed for ADR-004; #46 is dropped.

## Issue #45, one line

The owner's reason for dropping auto tare is not written anywhere, so it is left for the owner to give:

> Closing: auto tare is not planned. (reason)
