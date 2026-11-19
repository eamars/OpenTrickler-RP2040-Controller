// Set up (ADR-008, "Commissioning"; "The set-up, designed again", gate 4: the unit keeps its step, which the page tells
// it and its screen shows; the page and the 12864 move together): a unit from its first start, or a factory reset, to
// its first charge, in four
// steps: its Wi-Fi, its scale, its motors, ready. Each step can be skipped, and finishing or skipping the set-up is
// remembered by the unit (`set_up`, gate 4), so it opens by itself only on a unit never set up, on its own hotspot:
// offered, never forced, and no pop-up at every start. One column, everything in the page, nothing kept in the
// browser: it may run in a phone's sign-in sheet (web-ui-design, rule 13). Its API is proposed, played by the stand-in.
// features: 1 2 78 79 80 81 82 83 84 85 N7

const SETUP_STEPS = ["wifi", "scale", "motors", "ready"];
const SETUP_READ_MS = 700;      // the scale's weight and a join's progress, read while their step is shown
let setupTimer = null;
let setupLeft = false;          // the hotspot stopped at the end: the page is no longer on the unit's network

// A join that failed, said under the password (the unit's reason, by its network's name); a driver, in plain words
const JOIN_REFUSED = {
    wrong_password: () => "Wrong password. Check it and try again.",
    timeout: (ssid) => `${ssid} did not answer: move the unit nearer its router, or check it is a 2.4 GHz network.`,
    failed: (ssid) => `${ssid} did not let the unit join. Check its password and its security.`,
};
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

// --- Wi-Fi: one thing at a time (gate 4's design): the networks heard; a network's password; joining, its seconds
// counting; joined. A message stands under what it is about and goes as the person changes it. The hotspot stays up

// A network that needs a password: a lock, drawn in SVG's own namespace (an <svg> made as HTML draws nothing)
function lockIcon() {
    const span = element("span", {class: "lock", role: "img", "aria-label": "Needs a password"});
    span.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 11V8a5 5 0 0 1 10 0v3M5 11h14v10H5z"/></svg>';
    return span;
}

function networkRow(network, onChoose) {
    const row = element("button", {type: "button", class: "row"},
                        element("span", {}, network.ssid),
                        network.auth === "open" ? null : lockIcon(),
                        element("span", {class: "signal"}, signalWords(network.rssi)));
    row.onclick = () => onChoose(network);
    return row;
}

// A name or a password is taken as typed: a phone neither capitalises nor corrects it
const EXACT = {autocomplete: "off", autocapitalize: "none", autocorrect: "off", spellcheck: "false"};

async function drawWifiStep(page) {
    const [network, networks] = await Promise.all([api("GET", "/api/v1/network"), api("GET", "/api/v1/wifi/networks")]);
    const body = element("div", {class: "setup-body"});
    const state = {heard: networks.ok ? networks.data : {scanning: true, networks: []}};
    const show = (...parts) => body.replaceChildren(element("h2", {}, "Wi-Fi"), ...parts);

    // View 1: the networks heard, strongest first
    const listView = () => {
        tellStep("wifi", null);
        const list = element("div", {class: "list networks"});
        if (state.heard.scanning && state.heard.networks.length === 0) {
            list.append(element("p", {class: "row muted"}, "Looking for networks…"));
        }
        for (const one of state.heard.networks) {
            list.append(networkRow(one, (picked) => passwordView(picked, null)));
        }
        const other = networkRow({ssid: "Another network", rssi: 0, auth: "open"}, () => passwordView(null, null));
        other.querySelector(".signal").textContent = "Type its name";
        list.append(other);
        show(element("p", {class: "hint"}, "Choose your Wi-Fi network."), list,
             element("p", {class: "hint"}, "Not in the list? The unit hears 2.4 GHz networks only."),
             element("div", {class: "setup-actions"},
                     element("button", {class: "ghost", type: "button", onclick: scan}, "Look again"),
                     skipButton("wifi", "Skip: keep the unit's own hotspot")));
        state.view = "list";
    };

    // View 2: a network's password (and, for another network, its name and security); a refusal said under it
    const passwordView = (chosen, said) => {
        state.view = "password";
        state.chosen = chosen;
        if (chosen !== null) {
            tellStep("wifi", chosen.ssid);
        }
        const open = chosen !== null && chosen.auth === "open";
        const name = element("input", {type: "text", id: "join-ssid", maxlength: 31, ...EXACT});
        const security = element("select", {id: "join-auth"},
                                 ...[["wpa2_mixed", "WPA2"], ["wpa2_aes", "WPA2 (AES)"], ["wpa_tkip", "WPA (TKIP)"],
                                     ["open", "None"]].map(([value, label]) => new Option(label, value)));
        const password = element("input", {type: "password", id: "join-password", maxlength: 63, ...EXACT});
        const reveal = element("input", {type: "checkbox", id: "join-show"});
        reveal.onchange = () => password.type = reveal.checked ? "text" : "password";
        const error = element("p", {class: "field-error", hidden: said === null}, said || "");
        const clear = () => error.hidden = true;
        password.oninput = clear;
        name.oninput = clear;
        const join = element("button", {class: "action go", type: "button"}, "Join");
        join.onclick = () => {
            const ssid = chosen === null ? name.value.trim() : chosen.ssid;
            const auth = chosen === null ? security.value : chosen.auth;
            const typed = auth === "open" ? "" : password.value;
            if (!ssid) {
                error.textContent = "Type the network's name.";
                error.hidden = false;
                return;
            }
            if (auth !== "open" && (typed.length < 8 || typed.length > 63)) {
                error.textContent = "A Wi-Fi password has 8 to 63 characters.";
                error.hidden = false;
                return;
            }
            joinNetwork(ssid, typed, auth, error);
        };
        const fields = [];
        if (chosen === null) {
            fields.push(element("div", {class: "field"}, element("label", {for: "join-ssid"}, "Network name"), name),
                        element("div", {class: "field"}, element("label", {for: "join-auth"}, "Security"), security));
        }
        if (!open) {
            fields.push(element("div", {class: "field"}, element("label", {for: "join-password"}, "Password"), password,
                                element("label", {class: "show-key"}, reveal, " Show password")));
        }
        show(element("p", {class: "setup-network"}, chosen === null ? "Another network" : chosen.ssid),
             element("div", {class: "join"}, ...fields, error, join),
             element("div", {class: "setup-actions"},
                     element("button", {class: "ghost", type: "button", onclick: listView}, "Choose another network")));
        (chosen === null ? name : (open ? join : password)).focus();
    };

    // View 3: joining, its seconds counting, so the page is seen to move; the unit's screen counts with it
    const joinNetwork = async (ssid, password, auth, error) => {
        tellStep("wifi", ssid);
        const reply = await api("POST", "/api/v1/wifi/join", {ssid, password, auth});
        if (!reply.ok) {
            error.textContent = reply.data && reply.data.error ? reply.data.error.message : "The unit did not take it.";
            error.hidden = false;
            return;
        }
        joiningView(ssid, Date.now());
    };
    const joiningView = (ssid, since) => {
        state.view = "joining";
        const seconds = element("p", {class: "setup-count-up"});
        const count = () => seconds.textContent = `${Math.round((Date.now() - since) / 1000)} s of up to 30`;
        count();
        show(element("p", {class: "setup-status"}, `Joining ${ssid}`), seconds,
             element("p", {class: "hint"}, "The unit's screen shows it too."));
        const watch = async () => {
            if (state.view !== "joining") {
                return;
            }
            count();
            const now = await api("GET", "/api/v1/network");
            const s = now.ok ? now.data.station : null;
            if (s !== null && s.state === "joining" && typeof s.joining_ms === "number") {
                since = Date.now() - s.joining_ms;      // the unit's count, so both screens show the same seconds
                count();
            }
            if (s !== null && s.state === "joined") {
                joinedView(s, now.data.hostname);
                return;
            }
            if (s !== null && s.state === "failed") {
                const chosen = state.heard.networks.find((one) => one.ssid === s.ssid) || null;
                const said = JOIN_REFUSED[s.reason] || JOIN_REFUSED.failed;
                passwordView(chosen || {ssid: s.ssid, auth: "wpa2_mixed"}, said(s.ssid));
                return;
            }
            setupTimer = setTimeout(watch, 1000);     // joining, or the unit not heard: the contact check says so
        };
        watch();
    };

    // View 4: joined; the address it will have there, said again at the end
    const joinedView = (s, hostname) => {
        state.view = "joined";
        show(element("p", {class: "setup-status good"}, `Joined ${s.ssid}.`),
             element("p", {}, `On your Wi-Fi the unit will be ${hostname}.local (${s.address}).`),
             element("div", {class: "setup-actions"}, nextButton("wifi")));
    };

    const scan = async () => {
        const reply = await api("POST", "/api/v1/wifi/scan", {});
        if (reply.ok) {
            followScan();
        }
    };
    const followScan = async () => {
        const reply = await api("GET", "/api/v1/wifi/networks");
        if (!reply.ok) {
            return;
        }
        state.heard = reply.data;
        if (state.view === "list") {
            listView();     // a password being typed is never redrawn away
        }
        if (reply.data.scanning) {
            setupTimer = setTimeout(followScan, SETUP_READ_MS);
        }
    };

    page.replaceChildren(...setupFrame("wifi", body));
    // Where the unit is: joined (on the hotspot still, or set up again from its network), joining, or not yet
    const station = network.ok ? network.data.station : null;
    if (station !== null && station.state === "joined") {
        joinedView(station, network.data.hostname);
    } else if (station !== null && station.state === "joining") {
        joiningView(station.ssid, Date.now());
    } else {
        listView();
        if (!networks.ok || networks.data.networks.length === 0) {
            scan();
        }
    }
}

// --- Contact with the unit: through the set-up the page asks the unit its step every 2 s. Unheard for 5 s, the
// lost view says so (the phone may have left the hotspot); heard again, the set-up goes on from the unit's step

const SETUP_CONTACT_MS = 2000;
const SETUP_LOST_MS = 5000;
let setupHeardAt = Date.now();
let setupLost = false;
let setupContactTimer = null;
let setupHotspot = "opentrickler-…";    // the unit's hotspot, as its set-up names it

async function setupContact() {
    clearTimeout(setupContactTimer);
    if (pageShown !== "setup" || setupLeft) {
        return;
    }
    const reply = await api("GET", "/api/v1/setup");
    if (reply.ok) {
        setupHeardAt = Date.now();
        setupHotspot = reply.data.hotspot || setupHotspot;
        if (setupLost) {
            setupLost = false;
            const step = SETUP_STEPS.includes(reply.data.step) ? reply.data.step : "wifi";
            location.hash = `#setup/${step}`;
            PAGES.setup.enter(step);
        }
    } else if (!setupLost && Date.now() - setupHeardAt >= SETUP_LOST_MS) {
        setupLost = true;
        clearTimeout(setupTimer);
        $("page-setup").replaceChildren(...setupFrame("wifi", element("div", {class: "setup-body"},
            element("h2", {}, "Lost the unit"),
            element("p", {class: "setup-status bad"}, "Your phone may have left the unit's Wi-Fi."),
            element("p", {}, "The unit's screen shows how things stand."),
            element("p", {}, `Join ${setupHotspot} again in your phone's Wi-Fi settings: this page carries on by itself.`))));
    }
    setupContactTimer = setTimeout(setupContact, SETUP_CONTACT_MS);
}

// The step told to the unit as the page goes on, with the network chosen on the Wi-Fi step, so its screen shows the
// same stage
function tellStep(step, network) {
    api("PATCH", "/api/v1/setup", network === undefined ? {step} : {step, network});
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
        // Said before the hotspot stops, as this page, in a phone's sign-in sheet, may close with it
        body.append(element("p", {}, `Finishing, the unit leaves its hotspot for ${n.station.ssid}. Your phone goes back to your Wi-Fi; open the unit there at http://${n.hostname}.local, or http://${n.station.address}. The unit's screen shows the address too.`),
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
        tellStep(step);
        setupHeardAt = Date.now();
        setupContact();
        await {wifi: drawWifiStep, scale: drawScaleStep, motors: drawMotorsStep, ready: drawReadyStep}[step](page);
    },
    leave() {
        clearTimeout(setupTimer);
        clearTimeout(setupContactTimer);
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
