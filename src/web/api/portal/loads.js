// Loads (ADR-014): the eight charge profiles, each a name, a target and the powder it is thrown with, the one the
// charge throws marked. Each opens its form drawn from the schema (#loads/3), its powder chosen by name. Which load is
// thrown is chosen on the Trickler, as on the screen's Charge page. Titles as the powders': the navigation and the
// list's row say where the page is (web-ui-design, rule 16)

// features: N11
function drawLoadList(loads, inUse, chosen, powderNames, decimals) {
    const list = element("nav", {class: "list", "aria-label": "Loads"});
    for (const load of loads) {
        // Under the name, what it throws: its target and its powder. Nothing for a load that was not read
        const target = load.target === null ? "Target not set" : (load.target === undefined ? null : Number(load.target).toFixed(decimals));
        const sub = [target, powderNames[load.powder_profile]].filter(Boolean).join(" · ");
        list.append(element("a", {href: `#loads/${load.id}`, class: load.id === chosen ? "row two-line on" : "row two-line"},
                            element("span", {class: "row-text"}, element("span", {}, load.name),
                                    sub ? element("span", {class: "row-sub"}, sub) : null),
                            load.id === inUse ? element("span", {class: "tag in-use"}, "In use") : null,
                            load.saved === false ? element("span", {class: "tag"}, "Not stored") : null,
                            element("span", {class: "chevron", "aria-hidden": "true"}, "›")));
    }
    return list;
}

// features: N8
// Another client's write, the screen's or the Trickler's, shown here as it happens: a load written or stored, another
// load chosen (a choice is no event: the state says it)
function loadsFollow() {
    const page = $("page-loads");
    if (page.contains(document.activeElement) && document.activeElement.matches("input, select, textarea")) {
        return;     // a value being typed is never taken away; its save shows the unit's
    }
    route();
}

onUnit("event", (event) => pageShown === "loads" && event.source === "setting" && loadsFollow());
onUnit("state", (state) => {
    if (pageShown === "loads" && state.charge && state.charge.charge_profile !== PAGES.loads.inUse) {
        PAGES.loads.inUse = state.charge.charge_profile;    // one read for a change, however many states tell it
        loadsFollow();
    }
});
onUnit("afresh", () => pageShown === "loads" && route());

PAGES.loads = {
    drawing: 0,
    inUse: null,
    async enter(rest) {
        const page = $("page-loads");
        const drawing = ++this.drawing;     // the latest read alone is shown: one that ends after a newer is dropped
        const [loads, powders, charge, settings] = await Promise.all([
            readProfiles("/api/v1/charge_profiles"), api("GET", "/api/v1/powder_profiles"), api("GET", "/api/v1/charge"),
            api("GET", "/api/v1/settings/charge")]);
        if (drawing !== this.drawing) {
            return;
        }
        const inUse = charge.ok ? charge.data.charge_profile : null;
        this.inUse = inUse;
        const decimals = settings.ok && settings.data.decimal_places ? settings.data.decimal_places : 2;
        const powderChoices = powders.ok ? powders.data.profiles.map((p) => ({value: p.id, label: p.name})) : null;
        const powderNames = Object.fromEntries((powderChoices || []).map((choice) => [choice.value, choice.label]));
        // Side by side, an empty pane says nothing: the load in use is open (as the powders')
        const chosen = /^\d+$/.test(rest) ? Number(rest) : (SPLIT.matches ? inUse : null);
        const master = element("div", {class: "master"});
        const drawMaster = () => master.replaceChildren(drawLoadList(loads, inUse, chosen, powderNames, decimals));
        drawMaster();
        // The load shown, read again (another client's write): it stays as it is until the new is read whole, then
        // the two are swapped (web-ui-design, rule 17). A load newly asked for replaces the last at once
        const again = chosen !== null && page.dataset.chosen === String(chosen);
        if (again && (loads.failed || !charge.ok || !powders.ok)) {
            return;             // what is shown stays; the link's banner says the unit was not reached (rule 9)
        }
        const show = (...parts) => {
            page.dataset.open = chosen === null ? "list" : "part";
            page.dataset.chosen = chosen === null ? "" : String(chosen);
            page.replaceChildren(...parts);
        };
        if (!again) {
            show(master);
        }
        if (chosen === null) {
            return;
        }
        const reply = await api("GET", `/api/v1/charge_profiles/${chosen}`);
        // No title over the form: its first field is the load's name
        const pane = element("section", {class: "detail"}, element("a", {href: "#loads", class: "back"}, "‹ Loads"));
        if (reply.ok) {
            // A change shows in the list at once, from the write's own reply: its target, its powder, its "Not stored".
            // Store and Undo act on the eight and the choice together (one record in the EEPROM)
            const hooks = {
                changed: (values) => {
                    Object.assign(loads.find((load) => load.id === chosen), values);
                    drawMaster();
                },
                reload: () => route(),
                choices: powderChoices ? {powder_profile: powderChoices} : {},
            };
            pane.append(await drawForm(`/api/v1/charge_profiles/${chosen}`, reply.data, hooks));
        } else {
            pane.append(element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
        }
        if (drawing !== this.drawing || (again && !reply.ok)) {
            return;             // a newer read is shown, or this one failed and the load shown stays
        }
        again ? show(master, pane) : page.append(pane);
    },
    leave() {
    },
};
