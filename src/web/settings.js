// Settings (ADR-008): the unit's parts as a list, each opening its form drawn from the schema. On a phone the list,
// then a part's form in its place (#settings/scale); on a wide window the list beside the form, the first part open.
// Every setting a unit has is here: none is written into the page (features.md, rows 46 to 60). Where the page is
// is said once: the navigation marks the page, the list's row marks the part, so neither has a title of its own;
// only a part opened on a phone, where its row is not in view, has its name over it (web-ui-design, rule 16).

// The parts in the order a person sets a unit up; each one resource, but the motors, two under one name
const SETTINGS_PARTS = [
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

async function readCarries() {
    if (unitCarries === null) {
        const index = await api("GET", "/api/v1");
        unitCarries = index.ok ? Object.fromEntries((index.data.features || []).map((f) => [f.name, f.present])) : {};
    }
    return unitCarries;
}

async function drawSettingsList(resources, chosen) {
    const list = element("nav", {class: "list", "aria-label": "Settings"});
    for (const part of SETTINGS_PARTS) {
        const present = part.paths.filter((path) => resources[path]);
        if (present.length === 0 || (part.feature && unitCarries && unitCarries[part.feature] === false)) {
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
    leave() {},
};
