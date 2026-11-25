// Powders (ADR-008): the stored powders, the one the charge uses marked, each opening its form drawn from the schema
// (#powders/3), with a powder's file out and in (features.md, rows 61 to 65). On a wide window the list is a table
// of every powder's figures side by side, as the screen cannot show them; tuning them with powder is ADR-009's.
// Titles as the settings': the navigation and the list's row say where the page is (settings.js).

const POWDER_COLUMNS = [
    ["coarse_kp", "Kp"], ["coarse_ki", "Ki"], ["coarse_kd", "Kd"],
    ["fine_kp", "Kp"], ["fine_ki", "Ki"], ["fine_kd", "Kd"],
];

async function readPowders() {
    const list = await api("GET", "/api/v1/powder_profiles");
    if (!list.ok) {
        return [];
    }
    const replies = await Promise.all(list.data.profiles.map((p) => api("GET", `/api/v1/powder_profiles/${p.id}`)));
    return list.data.profiles.map((p, i) => Object.assign({id: p.id}, replies[i].ok ? replies[i].data : {name: p.name}));
}

// features: 62
function drawPowderList(powders, inUse, chosen) {
    const list = element("nav", {class: "list powders-list", "aria-label": "Powders"});
    for (const powder of powders) {
        list.append(element("a", {href: `#powders/${powder.id}`, class: powder.id === chosen ? "row on" : "row"},
                            element("span", {}, powder.name),
                            powder.id === inUse ? element("span", {class: "tag in-use"}, "In use") : null,
                            powder.saved === false ? element("span", {class: "tag"}, "Not stored") : null,
                            element("span", {class: "chevron", "aria-hidden": "true"}, "›")));
    }
    return list;
}

// Side by side: every powder's controller figures in one table, a row a powder (wide windows only)
function drawPowderTable(powders, inUse, chosen) {
    const head = element("tr", {}, element("th", {scope: "col"}, "Powder"),
                         ...POWDER_COLUMNS.map(([, label], i) => element("th", {scope: "col", class: i === 0 ? "split" : ""},
                                                                         label)));
    const group = element("tr", {class: "group-row"}, element("th"), element("th", {colspan: 3, class: "split"}, "Coarse"),
                          element("th", {colspan: 3, class: "split"}, "Fine"));
    const rows = powders.map((powder) => {
        const row = element("tr", {class: powder.id === chosen ? "on" : ""},
                            element("th", {scope: "row"},
                                    element("a", {href: `#powders/${powder.id}`}, powder.name),
                                    powder.id === inUse ? element("span", {class: "tag in-use"}, "In use") : null,
                                    powder.saved === false ? element("span", {class: "tag"}, "Not stored") : null),
                            ...POWDER_COLUMNS.map(([name], i) => element("td", {class: i % 3 === 0 ? "split" : ""},
                                                                         powder[name] === undefined ? "" : String(powder[name]))));
        row.onclick = (event) => {
            if (event.target.tagName !== "A") {
                location.hash = `#powders/${powder.id}`;
            }
        };
        return row;
    });
    return element("table", {class: "compare"}, element("thead", {}, group, head), element("tbody", {}, ...rows));
}

// features: 64
async function exportPowder(id) {
    const [index, powder] = await Promise.all([api("GET", "/api/v1"), api("GET", `/api/v1/powder_profiles/${id}`)]);
    if (!powder.ok) {
        refusal(powder, "Not exported");
        return;
    }
    const data = Object.assign({}, powder.data);
    delete data.saved;
    download(`${data.name || "powder"}.json`, {
        powder_profile: data,
        firmware: index.ok ? index.data.firmware.version : null,
        unit_id: index.ok ? index.data.unit_id : null,
    });
}

// A powder's file into this powder: only the fields this unit's schema has and may write; a field refused is named
// features: 65
async function importPowder(id) {
    const file = await chooseFile(".json,application/json");
    if (file === null) {
        return;
    }
    let data;
    try {
        const parsed = JSON.parse(await file.text());
        data = parsed.powder_profile || parsed;
    } catch (error) {
        say("Not imported: the file is not a powder's file");
        return;
    }
    const resource = await resourceOf(`/api/v1/powder_profiles/${id}`);
    const names = resource.fields.filter((f) => f.access === "read_write").map((f) => f.name);
    const body = Object.fromEntries(Object.entries(data).filter(([name]) => names.includes(name)));
    const reply = await api("PATCH", `/api/v1/powder_profiles/${id}`, body);
    if (!reply.ok) {
        refusal(reply, "Not imported");
        return;
    }
    say(`Imported into ${reply.data.name}: live, not yet stored.`, "good");
    route();
}

PAGES.powders = {
    async enter(rest) {
        const page = $("page-powders");
        const [powders, charge] = await Promise.all([readPowders(), api("GET", "/api/v1/charge")]);
        const inUse = charge.ok ? charge.data.powder_profile : null;
        // Side by side, an empty pane says nothing: the powder in use is open (as the settings' first part)
        const chosen = /^\d+$/.test(rest) ? Number(rest) : (SPLIT.matches ? inUse : null);
        page.dataset.open = chosen === null ? "list" : "part";
        const master = element("div", {class: "master"});
        // The tuning guide (features: 61 95): on GitHub
        const guide = element("p", {class: "guide"},
                              element("a", {href: "https://github.com/eamars/OpenTrickler/blob/main/Manual/tuning_guide.md",
                                            target: "_blank", rel: "noopener"}, "Tuning guide"),
                              " on GitHub");
        const drawMaster = () => master.replaceChildren(drawPowderList(powders, inUse, chosen), drawPowderTable(powders, inUse, chosen),
                                                        guide);
        drawMaster();
        page.replaceChildren(master);
        if (chosen === null) {
            return;
        }
        const reply = await api("GET", `/api/v1/powder_profiles/${chosen}`);
        // No title over the form: its first field is the powder's name
        const pane = element("section", {class: "detail"}, element("a", {href: "#powders", class: "back"}, "‹ Powders"));
        if (reply.ok) {
            // A change shows in the list and the table at once, from the write's own reply: this powder's figures and
            // its "Not stored". Store and Undo act on the eight together (one record in the EEPROM), which changes
            // nothing for a powder that was not changed
            const hooks = {
                changed: (values) => {
                    Object.assign(powders.find((powder) => powder.id === chosen), values);
                    drawMaster();
                },
                reload: () => route(),
            };
            pane.append(await drawForm(`/api/v1/powder_profiles/${chosen}`, reply.data, hooks));     // features: 63
            // ADR-009 step 1: this powder's calibration, its least speeds into the fields above once kept: its steps
            // here while it runs, its start among the powder's actions
            const calibrate = element("button", {class: "ghost", type: "button"}, "Calibrate this powder");
            pane.append(drawCalibration({profile: chosen, page: "powders", start: calibrate, saved: () => route()}));
            const files = element("div", {class: "button-row"}, calibrate,
                                  element("button", {class: "ghost", type: "button", onclick: () => exportPowder(chosen)}, "Export this powder"),
                                  element("button", {class: "ghost", type: "button", onclick: () => importPowder(chosen)}, "Import into this powder"));
            pane.append(files);
        } else {
            pane.append(element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
        }
        page.append(pane);
    },
    leave() {
        stopCalibrationPoll();
    },
};
