// Settings (ADR-008): the unit's parts as a list, each opening its form drawn from the schema. On a phone the list,
// then a part's form in its place (#settings/scale); on a wide window the list beside the form, the first part open.
// Every setting a unit has is here: none is written into the page (features.md, rows 46 to 60). Where the page is
// is said once: the navigation marks the page, the list's row marks the part, so neither has a title of its own;
// only a part opened on a phone, where its row is not in view, has its name over it (web-ui-design, rule 16).

// The parts in the order a person sets a unit up; each one resource, but the motors, two under one name
const SETTINGS_PARTS = [
    {key: "scale", label: "Scale", paths: ["/api/v1/settings/scale"]},
    {key: "charge", label: "Charge", paths: ["/api/v1/settings/charge"]},
    {key: "motors", label: "Motors", paths: ["/api/v1/settings/motors/coarse", "/api/v1/settings/motors/fine"],
     titles: ["Coarse", "Fine"]},
    {key: "gate", label: "Servo gate", paths: ["/api/v1/settings/gate"]},
    {key: "leds", label: "Lights", paths: ["/api/v1/settings/leds"]},
    {key: "display", label: "Display", paths: ["/api/v1/settings/display"]},
    {key: "wifi", label: "Wi-Fi", paths: ["/api/v1/settings/wifi"]},
    {key: "update", label: "Updates", paths: ["/api/v1/settings/update"]},
];

// Which of a part's resources is shown, by the switch at its top (the motors: Coarse or Fine), kept across a redraw
const partShown = {};

async function drawSettingsList(resources, chosen) {
    const list = element("nav", {class: "list", "aria-label": "Settings"});
    for (const part of SETTINGS_PARTS) {
        const present = part.paths.filter((path) => resources[path]);
        if (present.length === 0) {
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

async function drawSettingsPart(key) {
    const part = SETTINGS_PARTS.find((p) => p.key === key);
    const pane = element("section", {class: "detail"},
                         element("a", {href: "#settings", class: "back"}, "‹ Settings"),
                         element("h2", {}, part.label));
    const forms = [];
    for (const path of part.paths) {
        const reply = await api("GET", path);
        forms.push(reply.ok ? await drawForm(path, reply.data, () => route())
                            : element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
    }
    if (part.titles) {
        pane.append(drawSwitch(part, forms));
    }
    pane.append(...forms);
    return pane;
}

PAGES.settings = {
    async enter(rest) {
        const page = $("page-settings");
        const all = await api("GET", "/api/v1/settings");
        const resources = all.ok ? all.data.resources : {};
        let key = SETTINGS_PARTS.some((p) => p.key === rest) ? rest : null;
        if (key === null && SPLIT.matches) {
            // Side by side, an empty pane says nothing: the first part the unit carries is open
            const first = SETTINGS_PARTS.find((p) => p.paths.some((path) => resources[path]));
            key = first ? first.key : null;
        }
        page.dataset.open = key ? "part" : "list";
        page.replaceChildren(element("div", {class: "master"}, await drawSettingsList(resources, key)));
        if (key) {
            page.append(await drawSettingsPart(key));
        }
    },
    leave() {},
};
