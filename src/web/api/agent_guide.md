# OpenTrickler: a guide for an AI agent

You are helping a person who stands at this unit, a powder trickler for reloading. They load the cup and empty it; you read the throws and change the settings over this API. Everything below is on the unit itself: `GET /api/v1` (the index, its paths) and `GET /api/v1/schema` (every field, its limits, every action). Every POST takes a JSON body: `{}` when it has nothing. A held read waits 30 s at most (`wait=30`).

## First

1. Say hello: `POST /api/v1/agent/hello` with `{"name":"<who you are>"}` (1 to 16 characters). The screen shows the person who is watching. It is refused (409) while a charge runs: say it once the charge stops.
2. Ask the person once before you change anything: what you mean to do, and that a setting you try is live at once. Once they say yes, change and store what their goal needs, and tell them what you did and why.
3. Read what runs: `GET /api/v1/charge` (the load, its `target` and `powder_profile`), `GET /api/v1/powder_profiles/{id}`, `GET /api/v1/settings/charge`.

## The auto-tuner first

`settings/charge` `auto_tune`: on, the unit moves the fine kp and kd itself, judging windows of 25 throws at one target, and stores better gains into the powder's profile. It begins afresh at a restart, at a change of powder or target, and when the gains are changed. If it is off, offer it before tuning by hand. While you move the gains yourself, turn it off, and say so.

## Reading a throw

`GET /api/v1/charge/throws?after=N&wait=30` gives each throw's record after throw N, held until the next one is recorded. The schema's `throw` lists its fields. `simulated: true` is the unit's stand-in scale, which has no lag and no fall: never tune from it. On a dry run the person asks for, go through your steps as you would, and store nothing from it.

- **Over, with `fine_s` near 0:** the coarse tube poured past the target. Raise `coarse_stop_threshold` (`settings/charge`), or lower `coarse_kp`.
- **Over, with a high `fine_stop.rps`:** the fine tube was too fast at its stop. Lower `fine_kp`, by a tenth at a time, or `fine_min_speed_rps` by a quarter.
- **Over, with a large `fall`:** powder landed after the stop. Lower the fine tube's speed near the end, as above.
- **Good but slow (`elapsed_s` past 13 s, or `coarse_s` past 8 s):** raise `fine_kp` or `coarse_kp` a little.
- **Over, with `fine_ki` above 0:** lower `fine_ki` toward 0.

## What to move, and when to stop

The gains and speeds are the powder profile's (`powder_profiles/{id}`), the thresholds the charge's (`settings/charge`). Change one thing at a time and judge it over 10 throws: 9 good is done. Two overs in 10: lower `fine_kp` by a tenth, or `fine_min_speed_rps` by a quarter, at most three times, then tell the person what you found.

`fine_stop_threshold` is also the tolerance a throw is judged by: change it only at the person's word. To suggest one, take the median `fall` and a value half a scale step off a multiple of the step.

A setting is written by `PATCH`, live at once and not stored; it is refused (409 `busy`) while a throw runs, so write between a verdict and the cup's return. Each settings reply says whether it is `saved`.

## The end

Leave every resource stored (`POST` its `save` path) or put back (its `revert` path): `saved` true everywhere. Leave `auto_tune` as you found it unless the person chose otherwise. Tell the person in a few lines what you changed, and the throws that show it.

## Limits

- Start a charge (`POST /api/v1/charge/start`) only when the person says an empty cup is on the pan. The unit cannot see the cup or the hopper. `POST /api/v1/stop` stops whatever runs.
- A scale that sends nothing gives the unit nothing to read: check with the person its cable, its make (`settings/scale`) and its rate.
