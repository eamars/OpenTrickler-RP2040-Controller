// Settings (ADR-008): the unit's parts as a list, each opening its form drawn from the schema. On a phone the list,
// then a part's form in its place (#settings/scale); on a wide window the list beside the form, the first part open.
// Every setting a unit has is here: none is written into the page (features.md, rows 46 to 60). Where the page is
// is said once: the navigation marks the page, the list's row marks the part, so neither has a title of its own;
// only a part opened on a phone, where its row is not in view, has its name over it (web-ui-design, rule 16).

// The parts in the order a person sets a unit up; each one resource, but the motors, two under one name
const SETTINGS_PARTS = [
    {key: "calibration", label: "Calibration", paths: [], api: "/api/v1/calibration"},  // ADR-009 step 1
    {key: "scale", label: "Scale", paths: ["/api/v1/settings/scale"]},                  // features: 46 47
    {key: "charge", label: "Charge", paths: ["/api/v1/settings/charge"]},               // features: 49
    // features: 55 56
    {key: "motors", label: "Motors", paths: ["/api/v1/settings/motors/coarse", "/api/v1/settings/motors/fine"],
     titles: ["Coarse", "Fine"]},
    {key: "gate", label: "Servo gate", paths: ["/api/v1/settings/gate"], feature: "servo_gate"},    // features: 60
    {key: "leds", label: "Lights", paths: ["/api/v1/settings/leds"]},                   // features: 57
    {key: "display", label: "Display", paths: ["/api/v1/settings/display"]},            // features: 59 N4
    {key: "wifi", label: "Wi-Fi", paths: ["/api/v1/settings/wifi"]},                    // features: 53
    {key: "update", label: "Updates", paths: ["/api/v1/settings/update"]},              // features: N3
];

// Which of a part's resources is shown, by the switch at its top (the motors: Coarse or Fine), kept across a redraw
const partShown = {};

// features: 39
// What the unit carries, by its own report (the index's features: ADR-008, gate 2), read once: a part of a feature it
// does not carry is not drawn (features: N9)
let unitCarries = null;
// The paths the unit's index lists: a part with an API of its own (the calibration) is drawn only where the unit has it
let unitPaths = new Set();

async function readCarries() {
    if (unitCarries === null) {
        const index = await api("GET", "/api/v1");
        unitCarries = index.ok ? Object.fromEntries((index.data.features || []).map((f) => [f.name, f.present])) : {};
        unitPaths = new Set(index.ok ? (index.data.resources || []).map((r) => r.path) : []);
    }
    return unitCarries;
}

async function drawSettingsList(resources, chosen) {
    const list = element("nav", {class: "list", "aria-label": "Settings"});
    for (const part of SETTINGS_PARTS) {
        const present = part.paths.filter((path) => resources[path]);
        const listed = part.api ? unitPaths.has(part.api) : present.length > 0;
        if (!listed || (part.feature && unitCarries && unitCarries[part.feature] === false)) {
            continue;   // the unit does not carry it (ADR-005's report, gate 2): absent, not empty
        }
        const unsaved = present.some((path) => resources[path].saved === false);
        list.append(element("a", {href: `#settings/${part.key}`, class: part.key === chosen ? "row on" : "row"},
                            element("span", {}, part.label),
                            unsaved ? element("span", {class: "tag"}, "Not stored") : null,
                            element("span", {class: "chevron", "aria-hidden": "true"}, "›")));
    }
    return list;
}

// A part of more than one resource (the motors) shows one at a time, by a switch as the Trickler's
function drawSwitch(part, forms) {
    const shown = partShown[part.key] || 0;
    const buttons = part.titles.map((title, i) => element("button", {type: "button", role: "tab", class: i === shown ? "on" : "",
                                                                    "aria-selected": String(i === shown)}, title));
    buttons.forEach((button, i) => {
        button.onclick = () => {
            partShown[part.key] = i;
            buttons.forEach((b, j) => {
                b.classList.toggle("on", i === j);
                b.setAttribute("aria-selected", String(i === j));
            });
            forms.forEach((form, j) => {
                form.hidden = i !== j;
            });
        };
    });
    forms.forEach((form, j) => {
        form.hidden = j !== shown;
    });
    return element("div", {class: "seg", role: "tablist", "aria-label": part.label}, ...buttons);
}

// The gate moved now, beside its settings (features: 35 36 37): Open, Close, and how open it is, from the unit's own
// reading of it, never a value the page assumed. Shown as how open (0 to 100 %), as a person reads a gate; the API's
// ratio is how closed. Locked, and said why, while the gate is off in its settings
const GATE_STEP = 0.1;

function drawGateControl() {
    const open = element("button", {class: "ghost", type: "button"}, "Open");
    const close = element("button", {class: "ghost", type: "button"}, "Close");
    const less = element("button", {class: "key", type: "button", "aria-label": "Less open"}, "−");
    const more = element("button", {class: "key", type: "button", "aria-label": "More open"}, "+");
    const shown = element("output", {class: "gate-open", "aria-live": "polite"}, "--");
    const line = element("p", {class: "field-help"});
    let ratio = null;
    const draw = (gate) => {
        const off = gate === null || gate.state === "disabled";
        ratio = off ? null : gate.ratio;
        shown.textContent = ratio === null ? "--" : `${Math.round((1 - ratio) * 100)} %`;
        [open, close, less, more].forEach((button) => button.disabled = off);
        line.textContent = gate === null ? "Not read." :
            (off ? "Off: turn on Fitted below to move it." :
             (gate.state === "held" ? "Held where it was left: it moves again when set." : "Moves it now; nothing is stored."));
    };
    const refresh = async () => {
        const reply = await api("GET", "/api/v1/gate");
        draw(reply.ok ? reply.data : null);
    };
    const move = async (to) => {
        const reply = await api("PATCH", "/api/v1/gate", {ratio: Math.round(Math.min(Math.max(to, 0), 1) * 100) / 100});
        reply.ok ? draw(reply.data) : refusal(reply, "Not moved");
    };
    open.onclick = () => move(0);
    close.onclick = () => move(1);
    less.onclick = () => ratio !== null && move(ratio + GATE_STEP);
    more.onclick = () => ratio !== null && move(ratio - GATE_STEP);
    const card = element("div", {class: "fields gate-control"},
                         element("div", {class: "field"},
                                 element("label", {}, "How open"),
                                 element("div", {class: "stepper"}, less, shown, more),
                                 element("div", {class: "pair"}, open, close),
                                 line));
    card.refresh = refresh;
    refresh();
    return card;
}

// ADR-009 step 1: the calibration, its three parts each run alone, as the 12864's Settings > Calibration
// (tools/ui_mock/calibration_12864.py), on the unit's API (api_v1.md, Calibration; calibration.c). The stand-in plays
// it with no unit (tests/calibration_mock.py)
const CALIBRATION = "/api/v1/calibration";
// The fill's Hold to dispense: half a turn from now asked while held, more often than it takes to pour (a quarter second
// at the fill's 2 rev/s), so it pours steadily through a late reply; the unit extends a pour, never adds to it, so the
// tube stops within a quarter second of the release
const POUR_TURNS = 0.5;
const POUR_EVERY_MS = 150;
let calibrationTimer = null;
let calibrationOwner = null;    // the view whose card the poll draws into: one at a time
let calibrationPeriod = null;   // its poll's period, in ms

function stopCalibrationPoll() {
    if (calibrationTimer !== null) {
        clearInterval(calibrationTimer);
        calibrationTimer = null;
    }
    calibrationOwner = null;
    calibrationPeriod = null;
}

const known = (value) => value !== null && value !== undefined;

function weightText(value) {
    return known(value) ? `${value.toFixed(2)} gr` : "Not done";
}

function msText(ms) {
    return known(ms) ? `${(ms / 1000).toFixed(2)} s` : "Not done";
}

function rps(speed) {
    return known(speed) ? `${speed} rev/s` : "Not done";
}

function line(label, value) {
    return element("div", {class: "field inline"}, element("span", {}, label), element("output", {}, value));
}

// What changes within a step, as the unit says it: drawn into the step's card in place, never by rebuilding it
function liveValues(cal) {
    const live = {};
    if (["zeroing", "lift", "back", "fill"].includes(cal.step)) {
        live.figure = cal.weight.toFixed(2);
    }
    if (cal.step === "running") {
        live.figure = String(cal.progress);
        live.note = cal.line;
        live.bar = `${cal.progress}%`;
    }
    return live;
}

/*
 * The calibration's cards and its job's. In Settings (no options): the cup's and the scale's, and where a powder is
 * calibrated; a part's steps run in its own card, where its Calibrate button was (web-ui-design, rule 17: nothing
 * above them moves). On a powder's page ({profile, page: "powders", start, row, saved}): its steps while it runs, in
 * the place of row, the powder's actions (hidden meanwhile), and start, the page's own button among them, which starts
 * it and is off while any part runs; saved() once its records are kept, so its form shows them
 */
function drawCalibration(options = {}) {
    const forPowder = options.profile !== undefined;
    const page = options.page || "settings";
    const box = element("div", {class: "calibration"});
    let drawnKey = null;                // the step and the records the cards were built for
    const send = async (method, path, body) => {
        const reply = await api(method, path, body);
        reply.ok ? draw(reply.data) : refusal(reply, "Not done");
        if (reply.ok && path.endsWith("/save") && options.saved) {
            options.saved();
        }
    };
    const answer = (word) => send("POST", `${CALIBRATION}/answer`, {answer: word});
    const stop = () => send("POST", `${CALIBRATION}/stop`, {});
    const button = (text, kind, act) => element("button", {class: kind, type: "button", onclick: act}, text);
    const cancel = () => button("Cancel", "ghost", stop);
    // The fill's key: pours while it is held (a finger, the mouse, or Space and Enter), never after; a step that
    // replaces it under the finger stops it too
    const holdToPour = () => {
        const key = element("button", {class: "action ghost hold", type: "button"}, "Hold to dispense");
        let timer = null;
        const end = () => {
            if (timer !== null) {
                clearInterval(timer);
                timer = null;
            }
            key.classList.remove("held");
        };
        const pour = () => {
            if (!key.isConnected) {
                end();
                return;
            }
            api("POST", `${CALIBRATION}/pour`, {turns: POUR_TURNS});
        };
        const begin = (event) => {
            event.preventDefault();
            if (timer === null) {
                key.classList.add("held");
                pour();
                timer = setInterval(pour, POUR_EVERY_MS);
            }
        };
        key.addEventListener("pointerdown", begin);
        // A finger lifted or slid off the key, or the page left: the pour stops
        for (const name of ["pointerup", "pointercancel", "pointerleave", "blur"]) {
            key.addEventListener(name, end);
        }
        key.addEventListener("keydown", (event) => {
            if ((event.key === " " || event.key === "Enter") && !event.repeat) {
                begin(event);
            }
        });
        key.addEventListener("keyup", (event) => {
            if (event.key === " " || event.key === "Enter") {
                end();
            }
        });
        key.addEventListener("contextmenu", (event) => event.preventDefault());
        return key;
    };

    const jobCard = (cal, inCard) => {
        const card = element("div", {class: inCard ? "calibration-job in-card" : "fields calibration-job",
                                     "aria-live": "polite"});
        const head = (status) => card.append(element("div", {class: "status"}, element("span", {}, status)));
        const figure = (unit) => card.append(
            element("div", {class: "figure-line"}, element("span", {class: unit === "%" ? "figure short" : "figure",
                                                                    "data-live": "figure"}),
                    element("span", {class: "unit"}, unit)));
        const note = (text) => card.append(element("p", {class: "field-help", "data-live": text ? null : "note"},
                                                   text || ""));
        // A step's keys side by side; a step with one key gives it the card's width, as the Calibrate button had
        const actions = (...buttons) => card.append(element("div", {class: buttons.length === 1 ? "pair one" : "pair"},
                                                            ...buttons));
        const powder = cal.profile_name;
        const named = {cup: "The cup", scale: "The scale", powder}[cal.part];
        switch (cal.step) {
        case "cup_on":
            head("Put the empty cup on the scale");
            note(cal.part === "powder" ? `With ${powder} in the hopper: it is what is calibrated.` :
                 `To catch the powder. ${named} next.`);
            actions(cancel(), button("The cup is on", "action go", () => answer("next")));
            break;
        case "zeroing":
            head("Zeroing");
            figure("gr");
            actions(cancel());
            break;
        case "lift":
            head("Lift the cup");
            figure("gr");
            note("Its weight is read.");
            actions(cancel());
            break;
        case "back":
            head("Put the cup back");
            figure("gr");
            note(`The cup weighs ${weightText(cal.cup_weight)}.`);
            actions(cancel());
            break;
        case "fill":
            // As at the unit, where the knob pours (the owner, 2026-11-25): the key pours while held
            head("Fill the cup");
            figure("gr");
            card.append(holdToPour());
            actions(cancel(), button("It's full", "action go", () => answer("full")));
            break;
        case "keep":
            head(`It holds ${weightText(cal.capacity)}`);
            note("Keep it as the cup's capacity? No pour of a calibration passes it.");
            actions(cancel(), button("Keep", "action go", () => answer("keep")));
            break;
        case "empty":
            head("Empty the cup");
            note("Then put it back on the scale.");
            actions(button("Done", "action go", () => answer("done")));
            break;
        case "running":
            head("Calibrating");
            figure("%");
            card.append(element("div", {class: "bar"}, element("i", {"data-live": "bar"})));
            note();
            actions(button("Stop", "action halt", stop));
            break;
        case "paused":
            head("Empty the cup");
            note(`${cal.line} gr. Empty it, put it back, then go on.`);
            actions(button("Stop", "action halt", stop), button("Go on", "action go", () => answer("go_on")));
            break;
        case "result": {
            const result = cal.result;
            head(`${named}: done`);
            if (cal.part === "scale") {
                const lag = result.lag;
                card.append(line("Settle time", msText(result.settle_ms)),
                            line("Lag, slow flow", lag.length ? msText(lag[0].ms) : "Not found"),
                            line("Lag, fast flow", lag.length ? msText(lag[lag.length - 1].ms) : "Not found"));
            } else {
                card.append(line("Fine minimum", known(result.fine_min_speed_rps) ? rps(result.fine_min_speed_rps) :
                                                 "Not found"),
                            line("Was", rps(result.was.fine_min_speed_rps)),
                            line("Coarse minimum", known(result.coarse_min_speed_rps) ?
                                                   rps(result.coarse_min_speed_rps) : "Not found"),
                            line("Was", rps(result.was.coarse_min_speed_rps)));
            }
            card.append(line("Took", `${(result.elapsed_s / 60).toFixed(1)} min, ${result.poured.toFixed(0)} gr`));
            if (result.failure) {
                note(`${result.failure}.`);
            }
            actions(button("Discard", "ghost", stop),
                    button(cal.part === "scale" ? "Keep" : "Save", "action go",
                           () => send("POST", `${CALIBRATION}/save`, {})));
            break;
        }
        case "failed":
            // Said as a failure, never as something kept, as the 12864 says it
            head("Not calibrated");
            note(`${cal.failure}. Nothing changed.`);
            actions(button("OK", "action go", () => answer("done")));
            break;
        }
        return card;
    };

    const cards = (cal, job) => {
        const busy = cal.state !== "idle";
        const start = (part, text, extra = {}) =>
            element("button", {class: "ghost", type: "button", disabled: busy,
                               onclick: () => send("POST", `${CALIBRATION}/start`, {part, ...extra})}, text);
        if (forPowder) {
            return job ? [job] : [];        // in the place of the powder's actions (options.row)
        }
        const {cup, scale} = cal.records;
        const lag = scale.lag;
        // The part that runs shows its steps where its button was; the other's button is off meanwhile
        const slot = (part, text) => job && cal.part === part ? job : element("div", {class: "field"}, start(part, text));
        return [
            element("div", {class: "fields"}, element("h3", {class: "fields-title"}, "Cup"),
                    line("Capacity", weightText(cup.capacity)),
                    line("Weight", weightText(cup.weight)),
                    slot("cup", "Calibrate the cup")),
            element("div", {class: "fields"}, element("h3", {class: "fields-title"}, "Scale"),
                    line("Settle time", msText(scale.settle_ms)),
                    line("Lag, slow flow", lag.length ? msText(lag[0].ms) : "Not done"),
                    line("Lag, fast flow", lag.length ? msText(lag[lag.length - 1].ms) : "Not done"),
                    slot("scale", "Calibrate the scale")),
            element("div", {class: "fields"}, element("h3", {class: "fields-title"}, "Powders"),
                    element("p", {class: "field-help"}, "A powder is calibrated from its own page."),
                    element("div", {class: "field"}, element("button", {class: "ghost", type: "button",
                                                                        onclick: () => { location.hash = "#powders"; }},
                                                             "Open Powders"))),
        ];
    };

    if (options.start) {
        options.start.onclick = () => send("POST", `${CALIBRATION}/start`, {part: "powder", profile: options.profile});
    }
    const draw = (cal) => {
        if (options.start) {
            options.start.disabled = cal.state !== "idle";
        }
        // A powder's page shows its own powder's job alone
        const running = cal.state === "running" && cal.step !== null &&
                        (!forPowder || (cal.part === "powder" && cal.profile === options.profile));
        if (options.row) {
            options.row.hidden = running;
        }
        // Built again only for a new step or new records; within a step its figures change in place, so a button is
        // never replaced under a finger
        const key = JSON.stringify({step: running && [cal.part, cal.step, cal.capacity, cal.failure],
                                    state: cal.state, part: cal.part, profile: cal.profile, records: cal.records});
        if (key !== drawnKey) {
            drawnKey = key;
            const job = running ? jobCard(cal, !forPowder) : null;
            box.replaceChildren(...cards(cal, job));
            // A new step's question is never left above the screen, where the last step's taller card had it
            if (job !== null && job.getBoundingClientRect().top < 0) {
                job.scrollIntoView({block: "start"});
            }
        }
        if (running) {
            for (const [name, value] of Object.entries(liveValues(cal))) {
                const at = box.querySelector(`[data-live="${name}"]`);
                if (at === null) {
                    continue;
                }
                if (name === "bar") {
                    at.style.width = value;
                } else if (at.textContent !== value) {
                    at.textContent = value;
                }
            }
        }
        // One view polls at a time: the one on the page takes the poll over from any drawn before it. It polls with no
        // part running too, so a part started on the unit's screen or another browser is seen within 2 s
        const period = cal.state !== "idle" ? 500 : 2000;
        if (box.isConnected && (calibrationOwner !== box || calibrationPeriod !== period)) {
            stopCalibrationPoll();
            calibrationOwner = box;
            calibrationPeriod = period;
            calibrationTimer = setInterval(refresh, period);
        }
    };
    const refresh = async () => {
        if (pageShown !== page || (drawnKey !== null && !box.isConnected)) {
            if (calibrationOwner === box || pageShown !== page) {
                stopCalibrationPoll();
            }
            return;
        }
        const reply = await api("GET", CALIBRATION);
        if (reply.ok) {
            draw(reply.data);
        } else {
            drawnKey = null;
            box.replaceChildren(element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
        }
    };
    refresh();
    return box;
}

async function drawSettingsPart(key, hooks) {
    const part = SETTINGS_PARTS.find((p) => p.key === key);
    const pane = element("section", {class: "detail"},
                         element("a", {href: "#settings", class: "back"}, "‹ Settings"),
                         element("h2", {}, part.label));
    const forms = [];
    for (const path of part.paths) {
        const reply = await api("GET", path);
        forms.push(reply.ok ? await drawForm(path, reply.data, hooks)
                            : element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
    }
    if (part.titles) {
        pane.append(drawSwitch(part, forms));
    }
    if (key === "calibration") {
        pane.append(drawCalibration());
    }
    if (key === "gate") {
        // The gate's actions with its resource (README, "Handed on by ADR-010"); its Fitted, below, changes them
        const control = drawGateControl();
        hooks.also = () => control.refresh();
        pane.append(control);
    }
    pane.append(...forms);
    return pane;
}

// features: N8
// Another client's write, or the screen's, shown here as it happens (gate 3: every client echoes the unit)
onUnit("event", (event) => {
    if (pageShown !== "settings" || event.source !== "setting") {
        return;
    }
    const page = $("page-settings");
    if (page.contains(document.activeElement) && document.activeElement.matches("input, select, textarea")) {
        return;     // a value being typed is never taken away; its save shows the unit's
    }
    route();
});

onUnit("afresh", () => pageShown === "settings" && route());

PAGES.settings = {
    async enter(rest) {
        const page = $("page-settings");
        await readCarries();
        const all = await api("GET", "/api/v1/settings");
        const resources = all.ok ? all.data.resources : {};
        let key = SETTINGS_PARTS.some((p) => p.key === rest) ? rest : null;
        if (key === null && SPLIT.matches) {
            // Side by side, an empty pane says nothing: the first part the unit carries is open
            const first = SETTINGS_PARTS.find((p) => p.paths.some((path) => resources[path]));
            key = first ? first.key : null;
        }
        page.dataset.open = key ? "part" : "list";
        const master = element("div", {class: "master"}, await drawSettingsList(resources, key));
        page.replaceChildren(master);
        if (key) {
            // A change shows in the list at once: its row's "Not stored" from the write's own reply, nothing read again
            const hooks = {
                changed: (values, path) => {
                    resources[path].saved = values.saved;   // a part of two (the motors) is not stored while either is not
                    drawSettingsList(resources, key).then((list) => master.replaceChildren(list));
                    if (hooks.also) {
                        hooks.also();
                    }
                },
                reload: () => route(),
            };
            page.append(await drawSettingsPart(key, hooks));
        }
    },
    leave() {
        stopCalibrationPoll();
    },
};
