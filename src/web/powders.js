// Powders (ADR-008): the stored powders, the one the charge uses marked, each opening its form drawn from the schema
// (#powders/3), with a powder's file out and in (features.md, rows 61 to 65). On a wide window the list is a table
// of every powder's figures side by side, as the screen cannot show them; tuning them with powder is ADR-009's.

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
                                    powder.id === inUse ? element("span", {class: "tag in-use"}, "In use") : null),
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
        const chosen = /^\d+$/.test(rest) ? Number(rest) : null;
        const [powders, charge] = await Promise.all([readPowders(), api("GET", "/api/v1/charge")]);
        const inUse = charge.ok ? charge.data.powder_profile : null;
        page.dataset.open = chosen === null ? "list" : "part";
        page.replaceChildren(element("div", {class: "master"},
                                     element("h1", {}, "Powders"),
                                     drawPowderList(powders, inUse, chosen),
                                     drawPowderTable(powders, inUse, chosen)));
        if (chosen === null) {
            page.append(element("section", {class: "detail empty"}, element("p", {class: "hint"}, "Choose a powder to see or change its figures.")));
            return;
        }
        const reply = await api("GET", `/api/v1/powder_profiles/${chosen}`);
        const pane = element("section", {class: "detail"},
                             element("a", {href: "#powders", class: "back"}, "‹ Powders"),
                             element("h2", {}, reply.ok ? reply.data.name : `Powder ${chosen}`));
        if (reply.ok) {
            pane.append(await drawForm(`/api/v1/powder_profiles/${chosen}`, reply.data, () => route()));
            const files = element("div", {class: "button-row"},
                                  element("button", {class: "ghost", type: "button", onclick: () => exportPowder(chosen)}, "Export this powder"),
                                  element("button", {class: "ghost", type: "button", onclick: () => importPowder(chosen)}, "Import into this powder"));
            pane.append(files);
        } else {
            pane.append(element("p", {class: "hint"}, `Not read: ${refusalText(reply)}`));
        }
        page.append(pane);
    },
    leave() {},
};
