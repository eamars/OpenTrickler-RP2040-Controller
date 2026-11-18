// The Unit (ADR-008): what it is and its network; its firmware and the update (ADR-004, ADR-010); its screen, live;
// this browser's own preference; the backup and restore of every setting; restart and factory reset last, each
// asking first (features.md, rows 66 to 74, 86 to 90, and "New, not in today's pages").

const MIRROR_MS = 1000;
let mirrorTimer = null;

function fact(label, value) {
    return element("div", {class: "fact"}, element("dt", {}, label), element("dd", {}, value === null || value === undefined ? "--" : String(value)));
}

function uptimeText(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    return h > 0 ? `${h} h ${m} min` : `${m} min`;
}

function signalText(rssi) {
    if (typeof rssi !== "number") {
        return null;
    }
    const words = rssi >= -60 ? "strong" : (rssi >= -70 ? "good" : (rssi >= -80 ? "weak" : "very weak"));
    return `${words} (${rssi} dBm)`;
}

function card(title, ...children) {
    return element("section", {class: "card"}, element("h2", {}, title), ...children);
}

function drawFacts(index, network) {
    const net = network.ok ? network.data : {};
    const station = net.mode === "station";
    return card("This unit", element("dl", {class: "facts"},
                                     fact("Name", net.hostname || (index.ok ? index.data.hostname : null)),
                                     fact("Firmware", index.ok ? index.data.firmware.version : null),
                                     fact("Board", index.ok ? index.data.board : null),
                                     fact("Unit id", index.ok ? index.data.unit_id : null),
                                     fact(station ? "Network" : "Hotspot", net.ssid),
                                     fact("Address", net.address),
                                     station ? fact("Signal", signalText(net.rssi)) : null,
                                     fact("Running for", index.ok ? uptimeText(index.data.uptime_s) : null)));
}

// The update: what runs, what the poll found and the offer, a poll now, a file by hand (ADR-004's "too prosumer"
// list: the person needs to know nothing of slots or files to take an offer)
function drawUpdate(update) {
    const u = update.ok ? update.data : null;
    const body = [];
    if (u === null) {
        body.push(element("p", {class: "hint"}, `Not read: ${refusalText(update)}`));
        return card("Firmware update", ...body);
    }
    body.push(element("dl", {class: "facts"}, fact("Running", u.running.version)));
    if (u.found) {
        const offer = element("div", {class: u.found.critical ? "offer critical" : "offer"},
                              element("p", {class: "offer-title"}, `Version ${u.found.version} is available`,
                                      u.found.critical ? element("span", {class: "tag critical"}, "Important") : null),
                              u.found.note ? element("p", {}, u.found.note) : null,
                              u.found.bench ? element("p", {class: "hint"}, `From the bench host ${u.found.bench}`) : null,
                              element("button", {class: "action go", type: "button", onclick: installFound}, "Install"));
        body.push(offer);
    } else {
        const poll = u.poll || {};
        const said = poll.state === "failed" ? `The last check failed: ${poll.error}.` :
            (poll.state === "checking" ? "Checking..." : "No newer firmware found.");
        body.push(element("p", {class: "hint"}, said));
    }
    body.push(element("div", {class: "button-row"},
                      element("button", {class: "ghost", type: "button", onclick: checkUpdate}, "Check now"),
                      element("button", {class: "ghost", type: "button", onclick: () => installFile(u)}, "Install from a file"),
                      element("a", {class: "ghost link-button", href: "#settings/update"}, "Where updates come from")));
    return card("Firmware update", ...body);
}

async function checkUpdate() {
    const reply = await api("POST", "/api/v1/update/check", {});
    reply.ok ? say("Checking for newer firmware.", "good") : refusal(reply, "Not checked");
    setTimeout(route, 3000);
}

async function installFound() {
    const sure = await confirmAction("Install the update?",
                                     "The unit fetches it, checks it and restarts on it. Keep the power on until it is back. If the new firmware does not start, it goes back to the one it has now.",
                                     "Install", "Not now");
    if (!sure) {
        return;
    }
    const reply = await api("POST", "/api/v1/update/start", {download: true});
    reply.ok ? say("Updating: keep the power on.", "good") : refusal(reply, "Not started");
}

async function installFile(u) {
    say(`Choose the file ending in ${u.takes}_update.bin`, "good");
    const file = await chooseFile(".bin,application/octet-stream");
    if (file === null) {
        say("");
        return;
    }
    const sure = await confirmAction(`Install ${file.name}?`,
                                     "The unit takes the file, checks it is whole and for this board, and restarts on it. Keep the power on until it is back.",
                                     "Install", "Not now");
    if (!sure) {
        return;
    }
    const start = await api("POST", "/api/v1/update/start", {});
    if (!start.ok) {
        refusal(start, "Not started");
        return;
    }
    const sent = await api("PUT", "/api/v1/update/image", file);
    if (!sent.ok) {
        refusal(sent, "Not taken");
        return;
    }
    const installed = await api("POST", "/api/v1/update/install", {});
    installed.ok ? say("Installing: keep the power on.", "good") : refusal(installed, "Not installed");
}

// The unit's screen as a person at it sees it, read once a second while this page is shown
function drawMirror() {
    const canvas = element("canvas", {width: 128, height: 64, class: "mirror", "aria-label": "The unit's screen"});
    const frame = element("div", {class: "mirror-frame"}, canvas);
    return card("The unit's screen", frame);
}

async function readMirror() {
    const canvas = document.querySelector("#page-unit .mirror");
    if (canvas === null) {
        return;
    }
    try {
        const response = await fetch("/api/v1/display/frame");
        const bytes = new Uint8Array(await response.arrayBuffer());
        const context = canvas.getContext("2d");
        const image = context.createImageData(128, 64);
        for (let y = 0; y < 64; y++) {
            for (let x = 0; x < 128; x++) {
                const lit = (bytes[y * 16 + (x >> 3)] >> (7 - (x & 7))) & 1;
                const at = (y * 128 + x) * 4;
                const [r, g, b] = lit ? [221, 228, 234] : [28, 34, 40];
                image.data.set([r, g, b, 255], at);
            }
        }
        context.putImageData(image, 0, 0);
        setLink(true);
    } catch (error) {
        setLink(false);
    }
}

function drawBrowser() {
    const input = element("input", {type: "checkbox", id: "awake", checked: awakeWanted()});
    input.onchange = () => setAwakeWanted(input.checked);
    return card("This phone or computer",
                element("div", {class: "field inline"},
                        element("label", {for: "awake"}, "Keep this screen awake on the Trickler"),
                        element("label", {class: "switch"}, input, element("span")),
                        element("p", {class: "field-help"}, "Kept by this browser, not by the unit")));
}

async function exportSettings() {
    const [all, index] = await Promise.all([api("GET", "/api/v1/settings"), api("GET", "/api/v1")]);
    if (!all.ok) {
        refusal(all, "Not exported");
        return;
    }
    const id = index.ok ? index.data.unit_id : "unit";
    download(`opentrickler_${id}_settings.json`, all.data);
}

// Every resource in the file written with the fields this unit's schema has and may write; what is refused named
async function importSettings() {
    const file = await chooseFile(".json,application/json");
    if (file === null) {
        return;
    }
    let data;
    try {
        data = JSON.parse(await file.text());
    } catch (error) {
        say("Not restored: the file is not a settings file");
        return;
    }
    const left = [];
    for (const [path, values] of Object.entries(data.resources || {})) {
        const resource = await resourceOf(path);
        if (resource === null) {
            left.push(path);
            continue;
        }
        const names = resource.fields.filter((f) => f.access === "read_write").map((f) => f.name);
        const body = Object.fromEntries(Object.entries(values).filter(([name]) => names.includes(name)));
        const reply = await api("PATCH", path, body);
        if (!reply.ok) {
            left.push(`${resource.label}: ${refusalText(reply)}`);
        }
    }
    const stored = await api("POST", "/api/v1/settings/save", {});
    if (left.length > 0) {
        say(`Restored, but for: ${left.join("; ")}`);
    } else {
        stored.ok ? say("Restored and stored. Some settings take effect at the next restart.", "good") : refusal(stored, "Restored, not stored");
    }
}

async function restartUnit() {
    if (!await confirmAction("Restart the unit?", "It is back in about ten seconds. A charge or a purge that runs is stopped.",
                             "Restart", "Keep running")) {
        return;
    }
    const reply = await api("POST", "/api/v1/system/restart", {});
    reply.ok ? say("Restarting.", "good") : refusal(reply, "Not restarted");
}

async function factoryReset() {
    if (!await confirmAction("Erase every setting?",
                             "Every setting and powder goes back to its default, and it cannot be undone. The unit restarts as its own hotspot, with a new name: you join it again to set it up. Export the settings first to keep them.",
                             "Erase every setting", "Keep my settings")) {
        return;
    }
    const reply = await api("POST", "/api/v1/system/erase", {confirm: "erase"});
    reply.ok ? say("Erasing; the unit restarts as its own hotspot.", "good") : refusal(reply, "Not erased");
}

PAGES.unit = {
    async enter() {
        const page = $("page-unit");
        const [index, network, update] = await Promise.all([api("GET", "/api/v1"), api("GET", "/api/v1/network"),
                                                            api("GET", "/api/v1/update")]);
        page.replaceChildren(element("h1", {}, "Unit"),
                             element("div", {class: "cards"},
                                     drawFacts(index, network),
                                     drawUpdate(update),
                                     drawMirror(),
                                     drawBrowser(),
                                     card("Backup",
                                          element("p", {class: "hint"}, "Every setting and powder in one file, the Wi-Fi key left out."),
                                          element("div", {class: "button-row"},
                                                  element("button", {class: "ghost", type: "button", onclick: exportSettings}, "Export settings"),
                                                  element("button", {class: "ghost", type: "button", onclick: importSettings}, "Restore from a file"))),
                                     card("Restart and reset",
                                          element("div", {class: "button-row"},
                                                  element("button", {class: "ghost", type: "button", onclick: restartUnit}, "Restart"),
                                                  element("button", {class: "ghost danger", type: "button", onclick: factoryReset}, "Factory reset")))));
        readMirror();
        mirrorTimer = setInterval(readMirror, MIRROR_MS);
    },
    leave() {
        clearInterval(mirrorTimer);
        mirrorTimer = null;
    },
};
