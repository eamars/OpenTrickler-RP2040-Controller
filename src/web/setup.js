// Set up (ADR-008, "Commissioning"): a unit from its first start, or a factory reset, to its first charge, in four
// steps: its Wi-Fi, its scale, its motors, ready. Each step can be skipped, and finishing or skipping the set-up is
// remembered by the unit (`set_up`, gate 4), so it opens by itself only on a unit never set up, on its own hotspot:
// offered, never forced, and no pop-up at every start. One column, everything in the page, nothing kept in the
// browser: it may run in a phone's sign-in sheet (web-ui-design, rule 13). Its API is proposed, played by the stand-in.
// features: 1 2 78 79 80 81 82 83 84 85 N7

const SETUP_STEPS = ["wifi", "scale", "motors", "ready"];
const SETUP_READ_MS = 700;      // the scale's weight and a join's progress, read while their step is shown
let setupTimer = null;
let setupLeft = false;          // the hotspot stopped at the end: the page is no longer on the unit's network

// The unit's words for a join that failed and for a driver, in plain words
const JOIN_REFUSED = {wrong_key: "the key is wrong", not_found: "the network was not found",
                      timeout: "the network did not answer in time"};
const DRIVER_WORDS = {ok: "Driver found", absent: "Driver not found: check its cable and the board's power",
                      short: "A short on its motor's wires", hot: "Its driver is too hot", reset: "Its driver restarted",
                      supply: "Its driver's power is too low"};

function signalWords(rssi) {
    return rssi >= -60 ? "Strong" : (rssi >= -70 ? "Good" : (rssi >= -80 ? "Weak" : "Very weak"));
}

function setupFrame(step, ...body) {
    const at = SETUP_STEPS.indexOf(step) + 1;
    return [element("header", {class: "setup-head"}, element("h1", {}, "Set up this unit"),
                    element("span", {class: "setup-count"}, `${at} of ${SETUP_STEPS.length}`)),
            ...body];
}

function nextButton(step, label) {
    const next = SETUP_STEPS[SETUP_STEPS.indexOf(step) + 1];
    return element("a", {class: "action go link-button", href: `#setup/${next}`}, label || "Next");
}

function skipButton(step, label) {
    const next = SETUP_STEPS[SETUP_STEPS.indexOf(step) + 1];
    return element("a", {class: "ghost link-button skip", href: `#setup/${next}`}, label || "Skip this step");
}

async function finishSetup(then) {
    const reply = await api("POST", "/api/v1/setup/done", {});
    if (!reply.ok) {
        refusal(reply, "Not finished");
        return null;
    }
    if (then) {
        location.hash = then;
    }
    return reply.data;
}

// --- Wi-Fi: the networks the unit hears, one chosen and joined while the hotspot stays up, the result told here

// A network that needs a key: a lock, drawn in SVG's own namespace (an <svg> made as HTML draws nothing)
function lockIcon() {
    const span = element("span", {class: "lock", role: "img", "aria-label": "Needs a key"});
    span.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 11V8a5 5 0 0 1 10 0v3M5 11h14v10H5z"/></svg>';
    return span;
}

function networkRow(network, chosen, onChoose) {
    const row = element("button", {type: "button", class: chosen ? "row on" : "row"},
                        element("span", {}, network.ssid),
                        network.auth === "open" ? null : lockIcon(),
                        element("span", {class: "signal"}, signalWords(network.rssi)));
    row.onclick = () => onChoose(network);
    return row;
}

function joinForm(network, onJoin) {
    const other = network.ssid === null;
    const name = element("input", {type: "text", id: "join-ssid", autocomplete: "off", maxlength: 32});
    const security = element("select", {id: "join-auth"},
                             ...[["wpa2_mixed", "WPA2"], ["wpa2_aes", "WPA2 (AES)"], ["wpa_tkip", "WPA (TKIP)"], ["open", "None"]]
                                 .map(([value, label]) => new Option(label, value)));
    const key = element("input", {type: "password", id: "join-key", autocomplete: "off", maxlength: 63});
    const show = element("input", {type: "checkbox", id: "join-show"});
    show.onchange = () => key.type = show.checked ? "text" : "password";
    const needsKey = other || network.auth !== "open";
    const join = element("button", {class: "action go", type: "button"}, `Join ${other ? "" : network.ssid}`.trim());
    join.onclick = () => onJoin(other ? name.value.trim() : network.ssid, needsKey ? key.value : "",
                                other ? security.value : network.auth);
    return element("div", {class: "join"},
                   other ? element("div", {class: "field"}, element("label", {for: "join-ssid"}, "Network name"), name) : null,
                   other ? element("div", {class: "field"}, element("label", {for: "join-auth"}, "Security"), security) : null,
                   needsKey ? element("div", {class: "field"}, element("label", {for: "join-key"}, "Key"), key,
                                      element("label", {class: "show-key"}, show, " Show the key")) : null,
                   join);
}

async function drawWifiStep(page) {
    const [network, networks] = await Promise.all([api("GET", "/api/v1/network"), api("GET", "/api/v1/wifi/networks")]);
    const station = network.ok ? network.data.station : null;
    const body = element("div", {class: "setup-body"});
    // What the step shows: the networks heard, the one chosen (null: another, typed), a line on the last join. Kept
    // across each redraw, a scan's or a join's, so a choice and its key are never wiped under the person
    const state = {heard: networks.ok ? networks.data : {scanning: true, networks: []}, chosen: undefined, said: null};
    const joinedText = (s, hostname) => [element("p", {class: "setup-status good"}, `Joined ${s.ssid}.`),
                                         element("p", {}, `Its address there: ${hostname}.local (${s.address}).`),
                                         element("div", {class: "setup-actions"}, nextButton("wifi"))];
    const draw = () => {
        const parts = [element("h2", {}, "Wi-Fi")];
        if (station && station.state === "joined") {
            // Joined already: on its network (set up again: changed in Settings), or on the hotspot still
            if (network.data.mode === "station") {
                parts.push(element("p", {}, `On ${station.ssid}, at ${network.data.hostname}.local (${station.address}).`),
                           element("p", {class: "hint"}, "To join another network: Settings, Wi-Fi."),
                           element("div", {class: "setup-actions"}, nextButton("wifi")));
            } else {
                parts.push(...joinedText(station, network.data.hostname));
            }
            return body.replaceChildren(...parts);
        }
        parts.push(element("p", {class: "hint"}, "Choose your Wi-Fi. The unit keeps its hotspot until the set-up is done, so this page stays open."));
        if (state.said) {
            parts.push(state.said);
        }
        const list = element("div", {class: "list networks"});
        if (state.heard.scanning && state.heard.networks.length === 0) {
            list.append(element("p", {class: "row muted"}, "Looking for networks\u2026"));
        }
        const choose = (picked) => {
            Object.assign(state, {chosen: picked, said: null});
            draw();
            const field = $("join-key") || $("join-ssid");
            if (field) {
                field.focus();      // the key next, with no tap more
            }
        };
        const chosenName = state.chosen === undefined ? undefined : state.chosen.ssid;
        for (const one of state.heard.networks) {
            list.append(networkRow(one, chosenName === one.ssid, choose));
            if (chosenName === one.ssid) {
                list.append(joinForm(one, join));
            }
        }
        const other = {ssid: null, auth: "wpa2_mixed"};
        list.append(networkRow({ssid: "Another network", rssi: 0, auth: "open"}, chosenName === null, () => choose(other)));
        list.lastChild.querySelector(".signal").textContent = "Type its name";
        if (chosenName === null) {
            list.append(joinForm(other, join));
        }
        parts.push(list,
                   element("div", {class: "setup-actions"},
                           element("button", {class: "ghost", type: "button", onclick: scan}, "Look again"),
                           skipButton("wifi", "Skip: use its own hotspot")));
        body.replaceChildren(...parts);
    };
    const scan = async () => {
        const reply = await api("POST", "/api/v1/wifi/scan", {});
        reply.ok ? followScan() : refusal(reply, "Not looked");
    };
    const followScan = async () => {
        const reply = await api("GET", "/api/v1/wifi/networks");
        if (!reply.ok) {
            return;
        }
        state.heard = reply.data;
        // A key being typed is not redrawn away: the list is drawn again only while nothing is chosen
        if (state.chosen === undefined) {
            draw();
        }
        if (reply.data.scanning) {
            setupTimer = setTimeout(followScan, SETUP_READ_MS);
        }
    };
    // The join: tried while the hotspot stays up, followed to its end and said here; nothing restarts
    const join = async (ssid, key, auth) => {
        if (!ssid) {
            say("Type the network's name.");
            return;
        }
        const reply = await api("POST", "/api/v1/wifi/join", {ssid, key, auth});
        if (!reply.ok) {
            refusal(reply, "Not joined");
            return;
        }
        body.replaceChildren(element("h2", {}, "Wi-Fi"), element("p", {class: "setup-status"}, `Joining ${ssid}\u2026`));
        const watch = async () => {
            const now = await api("GET", "/api/v1/network");
            const s = now.ok ? now.data.station : null;
            if (s === null || s.state === "joining") {
                setupTimer = setTimeout(watch, SETUP_READ_MS);
                return;
            }
            if (s.state === "joined") {
                body.replaceChildren(element("h2", {}, "Wi-Fi"), ...joinedText(s, now.data.hostname));
                return;
            }
            // Refused: said over the list, the same network chosen again, its key to type again
            state.said = element("p", {class: "setup-status bad"}, `Not joined: ${JOIN_REFUSED[s.reason] || s.reason}. Try again.`);
            draw();
            const field = $("join-key");
            if (field) {
                field.focus();
            }
        };
        watch();
    };
    page.replaceChildren(...setupFrame("wifi", body));
    draw();
    if (!networks.ok || networks.data.networks.length === 0) {
        scan();
    }
}

// --- The scale: its make, and a live weight, which shows the scale talks; its port's speed when it does not

async function drawScaleStep(page) {
    const [settings, resource] = await Promise.all([api("GET", "/api/v1/settings/scale"),
                                                    resourceOf("/api/v1/settings/scale")]);
    const fields = resource ? resource.fields : [];
    const makeField = fields.find((f) => f.name === "driver");
    const baudField = fields.find((f) => f.name === "baud");
    const values = settings.ok ? settings.data : {};
    // Each choice written and stored at once: a set-up has no "not stored"
    const write = async (name, value) => {
        const reply = await api("PATCH", "/api/v1/settings/scale", {[name]: value});
        if (!reply.ok) {
            refusal(reply, "Not set");
            return;
        }
        Object.assign(values, reply.data);
        await api("POST", "/api/v1/settings/scale/save", {});
        silentFor = 0;
    };
    const make = makeField ? control("setup-make", makeField, values.driver, (value) => write("driver", value)) : null;
    const weight = element("p", {class: "setup-weight"}, element("span", {}, "---"), element("span", {class: "job-unit"}, ""));
    // The port's speed, drawn again after each choice so its mark follows what the unit holds
    const baudSlot = element("div");
    const drawBaud = () => baudField && baudSlot.replaceChildren(
        control("setup-baud", baudField, values.baud, async (value) => {
            await write("baud", value);
            drawBaud();
        }));
    drawBaud();
    const help = element("div", {class: "setup-help", hidden: true},
                         element("p", {}, "Nothing from the scale yet. Check its cable, and that the scale sends at the speed set here:"),
                         baudSlot);
    let silentFor = 0;
    const read = async () => {
        const reply = await api("GET", "/api/v1/scale");
        const w = reply.ok ? reply.data.weight : null;
        weight.firstChild.textContent = w === null ? "---" : w.toFixed(2);
        weight.lastChild.textContent = w === null ? "" : (reply.data.unit || "");
        silentFor = w === null ? silentFor + 1 : 0;
        help.hidden = silentFor < 4;        // about 3 s with no reading before it says so
        setupTimer = setTimeout(read, SETUP_READ_MS);
    };
    page.replaceChildren(...setupFrame("scale",
        element("div", {class: "setup-body"},
                element("h2", {}, "Scale"),
                element("div", {class: "field"}, element("label", {for: "setup-make"}, "Make"), make),
                element("div", {class: "field"}, element("label", {}, "Its weight now"), weight,
                        element("p", {class: "field-help"}, "A weight that moves as you touch the pan shows the scale talks to the unit.")),
                help,
                element("div", {class: "setup-actions"}, nextButton("scale"), skipButton("scale")))));
    read();
}

// --- The motors: each driver found or not, read from the unit; nothing to set here

async function drawMotorsStep(page) {
    const reply = await api("GET", "/api/v1/motors");
    const motors = reply.ok ? reply.data : {};
    const row = (name, label) => {
        const state = motors[name] ? motors[name].driver : null;
        return element("div", {class: state === "ok" ? "fact found" : "fact missing"},
                       element("dt", {}, label), element("dd", {}, state === null ? "Not read" : (DRIVER_WORDS[state] || state)));
    };
    const allFound = ["coarse", "fine"].every((name) => motors[name] && motors[name].driver === "ok");
    page.replaceChildren(...setupFrame("motors",
        element("div", {class: "setup-body"},
                element("h2", {}, "Motors"),
                element("dl", {class: "facts"}, row("coarse", "Coarse motor"), row("fine", "Fine motor")),
                allFound ? null : element("p", {class: "hint"}, "A driver not found is looked for again at each start, and in Settings, Motors."),
                element("div", {class: "setup-actions"}, nextButton("motors"), skipButton("motors")))));
}

// --- Ready: what was set, then the end. Joined to a network, the unit leaves its hotspot and says where to find it

async function drawReadyStep(page) {
    const [network, scale, motors] = await Promise.all([api("GET", "/api/v1/network"), api("GET", "/api/v1/scale"),
                                                        api("GET", "/api/v1/motors")]);
    const n = network.ok ? network.data : {station: {}};
    const joined = n.station && n.station.state === "joined";
    const onHotspot = n.mode === "access_point";
    const found = motors.ok && ["coarse", "fine"].every((name) => motors.data[name].driver === "ok");
    const facts = element("dl", {class: "facts"},
                          fact("Wi-Fi", joined ? n.station.ssid : "Its own hotspot"),
                          fact("Scale", scale.ok && scale.data.weight !== null ? "Sends its weight" : "No reading yet"),
                          fact("Motors", found ? "Both drivers found" : "A driver not found"));
    const body = element("div", {class: "setup-body"}, element("h2", {}, "Ready"), facts);
    if (joined && onHotspot) {
        const finish = element("button", {class: "action go", type: "button"}, "Finish: go to your Wi-Fi");
        finish.onclick = async () => {
            if (await finishSetup(null) === null) {
                return;
            }
            setupLeft = true;   // the unit leaves its hotspot: this page loses it, as it should
            const address = `http://${n.hostname}.local`;
            body.replaceChildren(element("h2", {}, "Done"),
                                 element("p", {}, `The unit is on ${n.station.ssid} now and has left its hotspot. Your phone goes back to your Wi-Fi by itself; then open the unit there:`),
                                 element("a", {class: "setup-address", href: address}, address),
                                 element("p", {class: "hint"}, `or http://${n.station.address}`),
                                 element("p", {}, "Then put the empty cup on the scale and try a first charge."));
        };
        body.append(element("p", {class: "hint"}, "Finishing, the unit leaves its hotspot for your Wi-Fi, and this page closes with it."),
                    element("div", {class: "setup-actions"}, finish));
    } else {
        body.append(element("p", {class: "hint"}, "Put the empty cup on the scale and try a first charge."),
                    element("div", {class: "setup-actions"},
                            element("button", {class: "action go", type: "button", onclick: () => finishSetup("#trickler")},
                                    "Start using it")));
    }
    page.replaceChildren(...setupFrame("ready", body));
}

PAGES.setup = {
    async enter(rest) {
        clearTimeout(setupTimer);
        const step = SETUP_STEPS.includes(rest) ? rest : "wifi";
        const page = $("page-setup");
        if (setupLeft) {
            return;     // finished, the unit gone to its network: the last word stays
        }
        await {wifi: drawWifiStep, scale: drawScaleStep, motors: drawMotorsStep, ready: drawReadyStep}[step](page);
    },
    leave() {
        clearTimeout(setupTimer);
    },
    // Shown again (a phone woken, another app left): the step stays as it was, a key typed included
    resume() {},
};

// The set-up opens by itself on a unit never set up, on its own hotspot, when the page is opened at its root
async function offerSetup() {
    if (location.hash) {
        return;
    }
    const [system, network] = await Promise.all([api("GET", "/api/v1/system"), api("GET", "/api/v1/network")]);
    if (system.ok && network.ok && system.data.set_up === false && network.data.mode === "access_point") {
        history.replaceState(null, "", "#setup");
    }
}
