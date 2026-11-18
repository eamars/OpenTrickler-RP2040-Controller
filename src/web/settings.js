// Settings (ADR-008): the unit's parts as a list, each opening its form drawn from the schema. On a phone the list,
// then a part's form in its place (#settings/scale); on a wide window the list beside the form. Every setting a
// unit has is here: none is written into the page (features.md, rows 46 to 60).

// The parts in the order a person sets a unit up; each one resource, but the motors, two under one name
const SETTINGS_PARTS = [
    {key: "scale", label: "Scale", paths: ["/api/v1/settings/scale"]},
    {key: "charge", label: "Charge", paths: ["/api/v1/settings/charge"]},
    {key: "motors", label: "Motors", paths: ["/api/v1/settings/motors/coarse", "/api/v1/settings/motors/fine"],
     titles: ["Coarse motor", "Fine motor"]},
    {key: "gate", label: "Servo gate", paths: ["/api/v1/settings/gate"]},
    {key: "leds", label: "Lights", paths: ["/api/v1/settings/leds"]},
    {key: "display", label: "Display", paths: ["/api/v1/settings/display"]},
    {key: "wifi", label: "Wi-Fi", paths: ["/api/v1/settings/wifi"]},
    {key: "update", label: "Updates", paths: ["/api/v1/settings/update"]},
];

async function drawSettingsList(chosen) {
    const all = await api("GET", "/api/v1/settings");
    const resources = all.ok ? all.data.resources : {};
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

async function drawSettingsPart(key) {
    const part = SETTINGS_PARTS.find((p) => p.key === key);
    const pane = element("section", {class: "detail"},
                         element("a", {href: "#settings", class: "back"}, "‹ Settings"),
                         element("h2", {}, part.label));
    for (let i = 0; i < part.paths.length; i++) {
        const reply = await api("GET", part.paths[i]);
        if (!reply.ok) {
            pane.append(element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
            continue;
        }
        if (part.titles) {
            pane.append(element("h3", {}, part.titles[i]));
        }
        pane.append(await drawForm(part.paths[i], reply.data, () => route()));
    }
    return pane;
}

PAGES.settings = {
    async enter(rest) {
        const page = $("page-settings");
        const key = SETTINGS_PARTS.some((p) => p.key === rest) ? rest : null;
        const list = await drawSettingsList(key);
        page.dataset.open = key ? "part" : "list";
        const body = element("div", {class: "master"}, element("h1", {}, "Settings"), list);
        page.replaceChildren(body);
        if (key) {
            page.append(await drawSettingsPart(key));
        } else {
            page.append(element("section", {class: "detail empty"}, element("p", {class: "hint"}, "Choose a part of the unit to set it.")));
        }
    },
    leave() {},
};
