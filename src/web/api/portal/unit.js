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

// What the unit is, under its own name; its firmware's version is said once, in the update's card
// features: 66
function drawFacts(index, network) {
    const net = network.ok ? network.data : {};
    const station = net.mode === "station";
    const name = net.hostname || (index.ok ? index.data.hostname : null) || "This unit";
    return card(name, element("dl", {class: "facts"},
                              fact("Board", index.ok ? index.data.board : null),
                              fact("Unit id", index.ok ? index.data.unit_id : null),
                              fact("Build", index.ok ? `${index.data.firmware.build}, ${index.data.firmware.vcs}` : null),
                              fact(station ? "Network" : "Hotspot", net.ssid),
                              fact("Address", net.address),
                              station ? fact("Signal", signalText(net.rssi)) : null,
                              fact("Running for", index.ok ? uptimeText(index.data.uptime_s) : null)),
                element("div", {class: "button-row"},
                        element("a", {class: "ghost link-button", href: "#setup"}, "Set up this unit")));
}

// The unit's screen as a person at it sees it, read once a second while this page is shown, at the size and in the
// format the unit gives (features: 86 87 88 90): another screen is drawn right, or said not drawn
function drawMirror(display) {
    const d = display.ok ? display.data : null;
    if (d === null || d.format !== "mono_rows") {
        return card("The unit's screen", element("p", {class: "hint"},
                    d === null ? "Not read." : `Not drawn here: its format is ${d.format}.`));
    }
    const canvas = element("canvas", {width: d.width, height: d.height, class: "mirror", "aria-label": "The unit's screen"});
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
        const [width, height] = [canvas.width, canvas.height];
        const image = context.createImageData(width, height);
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const lit = (bytes[y * (width >> 3) + (x >> 3)] >> (7 - (x & 7))) & 1;
                const at = (y * width + x) * 4;
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

// features: N6
function drawBrowser() {
    const input = element("input", {type: "checkbox", id: "awake", checked: awakeWanted()});
    input.onchange = () => setAwakeWanted(input.checked);
    return card("This phone or computer",
                element("div", {class: "field inline"},
                        element("label", {for: "awake"}, "Keep this screen awake on the Trickler"),
                        element("label", {class: "switch"}, input, element("span")),
                        element("p", {class: "field-help"}, "Kept by this browser, not by the unit")));
}

// features: 71
async function exportSettings() {
    const [all, index] = await Promise.all([api("GET", "/api/v1/settings"), api("GET", "/api/v1")]);
    if (!all.ok) {
        refusal(all, "Not exported");
        return;
    }
    const id = index.ok ? index.data.unit_id : "unit";
    download(`opentrickler_${id}_settings.json`, all.data);
}

// A backup from before API v1 (features: 73): each old path's codes read as v1's fields, so a person's file from an
// older firmware still restores. [the v1 path, {code: field, or [field, the values by their old index]}]
// The scale's make by its stored number, the release's (tests/release_records.py checks it): 9 is FA-E/EN
const OLD_MOTOR = {m0: "acceleration_rps2", m1: "full_steps_per_rotation", m2: "current_ma", m3: "microsteps",
                   m4: "max_speed_rps", m5: "r_sense_mohm", m6: "min_speed_rps", m7: "gear_ratio", m8: "invert_enable",
                   m9: "invert_direction"};
const OLD_MOTOR_NAMES = {"/rest/coarse_motor_config": "Coarse", "/rest/fine_motor_config": "Fine"};
const OLD_PATHS = {
    "/rest/scale_config": ["/api/v1/settings/scale", {
        s0: ["driver", ["and_fxi", "steinberg_sbs", "gng_jjb", "ussolid_jfdbs", "jm_science", "creedmoor", "radwag_ps_r2",
                        "sartorius", "generic", "fa_en"]],
        s1: ["baud", [4800, 9600, 19200]], s2: ["format", ["8n1", "7n1"]]}],
    "/rest/charge_mode_config": ["/api/v1/settings/charge", {
        c1: "colour_good", c2: "colour_under", c3: "colour_over", c4: "colour_not_ready", c5: "coarse_stop_threshold",
        c6: "fine_stop_threshold", c7: "steady_sd_margin", c8: "zero_mean_margin", c9: ["decimal_places", [2, 3]],
        c10: "precharge_enable", c11: "precharge_time_ms", c12: "precharge_speed_rps", c13: "coarse_stop_gate_ratio",
        c14: "coarse_stop_backoff_enable", c15: "coarse_stop_backoff_turns", c16: "coarse_stop_backoff_speed_rps"}],
    "/rest/coarse_motor_config": ["/api/v1/settings/motors/coarse", OLD_MOTOR],
    "/rest/fine_motor_config": ["/api/v1/settings/motors/fine", OLD_MOTOR],
    "/rest/mini_12864_config": ["/api/v1/settings/display", {b0: "invert_encoder", b1: ["rotation", [0, null, 180, null]]}],
    "/rest/wireless_config": ["/api/v1/settings/wifi", {
        w0: "ssid", w2: ["auth", ["open", "wpa_tkip", "wpa2_aes", "wpa2_mixed"]], w3: "timeout_ms", w4: "enable"}],
    "/rest/neopixel_led_config": ["/api/v1/settings/leds", {
        bl: "backlight_colour", l1: "led1_colour", l2: "led2_colour", l3: "pwm3_chain_count", l4: "pwm3_rgbw",
        l5: ["pwm3_colour_order", ["rgb", "grb"]]}],
    "/rest/servo_gate_config": ["/api/v1/settings/gate", {
        c0: "enable", c1: "shutter0_close_duty", c2: "shutter0_open_duty", c3: "shutter1_close_duty",
        c4: "shutter1_open_duty", c5: "close_speed_per_s", c6: "open_speed_per_s"}],
};
const OLD_PROFILE = {p2: "name", p3: "coarse_kp", p4: "coarse_ki", p5: "coarse_kd", p6: "coarse_min_speed_rps",
                     p7: "coarse_max_speed_rps", p8: "fine_kp", p9: "fine_ki", p10: "fine_kd", p11: "fine_min_speed_rps",
                     p12: "fine_max_speed_rps"};

// An old file's motor current is the figure before ADR-012, the current only with the driver's trimmer at full scale:
// converted as the firmware converts a stored one (motors.c), by the factory trimmer's share, to 10 mA
const OLD_TRIMMER_SHARE = 0.48;

function trueCurrent(oldMa) {
    return Math.round(oldMa * OLD_TRIMMER_SHARE / 10) * 10;
}

// {resources: {v1 path: fields}, currents: [[v1 path, the motor's name, the file's figure, the figure written]]}
function oldBackupToV1(config) {
    const resources = {};
    const currents = [];
    for (const [endpoint, values] of Object.entries(config)) {
        const profile = endpoint.match(/^\/rest\/profile_config\?pf=(\d+)$/);
        const [path, codes] = profile ? [`/api/v1/powder_profiles/${profile[1]}`, OLD_PROFILE] : (OLD_PATHS[endpoint] || [null, null]);
        if (path === null) {
            continue;
        }
        resources[path] = {};
        for (const [code, value] of Object.entries(values)) {
            const field = codes[code];
            if (Array.isArray(field)) {
                const mapped = field[1][value];
                if (mapped !== undefined && mapped !== null) {
                    resources[path][field[0]] = mapped;
                }
            } else if (field === "coarse_stop_gate_ratio" && value < 0) {
                resources[path][field] = null;      // -1: the gate is not moved
            } else if (field === "current_ma") {
                resources[path][field] = trueCurrent(value);
                currents.push([path, OLD_MOTOR_NAMES[endpoint], value, resources[path][field]]);
            } else if (field) {
                resources[path][field] = value;
            }
        }
    }
    return {resources, currents};
}

// The currents of an old file, converted: said once, after its restore, with the way to raise them (the owner,
// 2026-11-28)
function tellCurrents(version, currents) {
    const dialog = $("currents");
    $("currents-from").textContent = `Firmware ${version || "1"} counted the motors' current another way. Each figure ` +
                                     "was converted to the same real current, for a driver whose trimmer was left as it came:";
    $("currents-figures").replaceChildren(...currents.map(([, name, from, to]) =>
        element("span", {}, `${name} ${from} → ${to} mA`)));
    dialog.returnValue = "";
    dialog.onclose = () => {
        if (dialog.returnValue === "motors") {
            location.hash = "#settings/motors";
        }
    };
    dialog.showModal();
}

// Every resource in the file written with the fields this unit's schema has and may write; what is refused named
// features: 72 74
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
    const old = data.resources ? null : oldBackupToV1(data.config || {});
    const left = [];
    const written = new Set();
    for (const [path, values] of Object.entries(data.resources || old.resources)) {
        const resource = await resourceOf(path);
        if (resource === null) {
            left.push(path);
            continue;
        }
        const names = resource.fields.filter((f) => f.access === "read_write").map((f) => f.name);
        const body = Object.fromEntries(Object.entries(values).filter(([name]) => names.includes(name)));
        const reply = await api("PATCH", path, body);
        if (reply.ok) {
            written.add(path);
        } else {
            left.push(`${resource.label}: ${refusalText(reply)}`);
        }
    }
    const stored = await api("POST", "/api/v1/settings/save", {});
    if (left.length > 0) {
        say(`Restored, but for: ${left.join("; ")}`);
    } else {
        stored.ok ? say("Restored and stored. Some settings take effect at the next restart.", "good") : refusal(stored, "Restored, not stored");
    }
    const currents = old === null ? [] : old.currents.filter(([path]) => written.has(path));
    if (currents.length > 0) {
        tellCurrents(data.firmware_version, currents);
    }
}

// features: 68 70
async function restartUnit() {
    if (!await confirmAction("Restart the unit?", "It is back in about ten seconds. A charge or a purge that runs is stopped.",
                             "Restart", "Keep running")) {
        return;
    }
    const index = await api("GET", "/api/v1");
    const reply = await api("POST", "/api/v1/system/restart", {});
    if (!reply.ok) {
        refusal(reply, "Not restarted");
        return;
    }
    followRestart(index.ok ? index.data.boot : null);     // the page follows it to its new boot (update.js)
    watchJobs();
}

// features: 69 70
async function factoryReset() {
    if (!await confirmAction("Erase every setting?",
                             "Every setting and powder goes back to its default, and it cannot be undone. The unit restarts as its own hotspot: you join it again to set it up. Export the settings first to keep them.",
                             "Erase every setting", "Keep my settings", true)) {
        return;
    }
    const reply = await api("POST", "/api/v1/system/factory_reset", {confirm: "factory_reset"});
    reply.ok ? say("Erasing; the unit restarts as its own hotspot.", "good") : refusal(reply, "Not erased");
}

PAGES.unit = {
    async enter() {
        const page = $("page-unit");
        const [index, network, update, display] = await Promise.all([api("GET", "/api/v1"), api("GET", "/api/v1/network"),
                                                                     api("GET", "/api/v1/update"), api("GET", "/api/v1/display")]);
        // The update first, where its mark on the navigation leads, and in the same place with or without an offer
        page.replaceChildren(element("div", {class: "cards"},
                                     drawUpdateCard(update),
                                     drawFacts(index, network),
                                     drawMirror(display),
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
