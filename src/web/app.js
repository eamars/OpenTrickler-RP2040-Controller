// The charge's page (ADR-008, gate 1's mock): reads the unit and acts through API v1. The same script runs against the
// stand-in (tests/web_test.py) and the unit. Polling for now; the event stream replaces it in gate 2.
"use strict";

const $ = (id) => document.getElementById(id);
const POLL_MS = 500;
const PURGE_RESEND_MS = 2000;   // a purge's speed lasts 5 s on the unit: a page that goes leaves no motor turning
const PURGE_STEP = 0.5;
const PURGE_MAX = 5;

let charge = null;
let purge = null;
let decimals = 2;
let purgeSpeed = 0;
let purgeTimer = null;
let pollTimer = null;
let wakeLock = null;

async function api(method, path, body) {
    try {
        const response = await fetch(path, {
            method,
            headers: body === undefined ? {} : {"Content-Type": "application/json"},
            body: body === undefined ? undefined : JSON.stringify(body),
        });
        const data = await response.json().catch(() => null);
        setLink(true);
        return {ok: response.ok, status: response.status, data};
    } catch (error) {
        setLink(false);
        return {ok: false, status: 0, data: null};
    }
}

function say(text) {
    $("message").textContent = text;
}

function refusal(reply, what) {
    const message = reply.data && reply.data.error ? reply.data.error.message : "the unit was not reached";
    say(`${what}: ${message}`);
}

function setLink(reached) {
    const link = $("link");
    link.querySelector("b").textContent = reached ? "Connected" : "Unit not reached";
    link.className = reached ? "link up" : "link down";
}

// The screen's job titles, word for word (charge_state_text() in menu_nav.c)
const STATUS = {idle: "Ready", starting: "Starting", zeroing: "Zeroing", charging: "Charging",
                waiting_for_cup_removal: "Lift the cup", waiting_for_cup_return: "Return the cup", fault: "Fault"};
const VERDICT = {good: "Good", under: "Under", over: "Over"};

function shown(value) {
    return value === null || value === undefined ? "---" : Number(value).toFixed(decimals);
}

function renderCharge() {
    if (charge === null) {
        return;
    }
    const running = charge.state !== "idle" && charge.state !== "fault";
    $("weight").textContent = shown(charge.weight);
    $("target-shown").textContent = shown(charge.target);
    const share = charge.target && charge.weight ? Math.min(Math.max(charge.weight / charge.target, 0), 1) : 0;
    $("ring").style.strokeDashoffset = String(1000 * (1 - share));
    $("bar").style.width = `${share * 100}%`;
    $("elapsed").textContent = charge.elapsed_s === null ? "-.-- s" : `${charge.elapsed_s.toFixed(2)} s`;
    if (document.activeElement !== $("target") && charge.target !== null) {
        $("target").value = Number(charge.target).toFixed(decimals);
    }
    if (document.activeElement !== $("powder")) {
        $("powder").value = String(charge.powder_profile);
    }
    $("powder").disabled = running;
    $("target").disabled = running;
    $("start").hidden = running;
    $("stop").hidden = !running;

    const order = [...$("steps").children].map((step) => step.dataset.state);
    const reached = order.indexOf(charge.state);
    [...$("steps").children].forEach((step, index) => {
        step.classList.toggle("done", reached >= 0 && index < reached);
        step.classList.toggle("now", index === reached);
    });

    // Colour only where it means something (web-ui-design, rule 5): not ready, then the throw's verdict
    const result = charge.last && charge.last.result;
    const afterThrow = charge.state === "waiting_for_cup_removal" || (!running && result in VERDICT);
    let mood = "normal";
    let status = STATUS[charge.state] || charge.state;
    if (charge.state === "fault") {
        mood = "over";
    } else if (afterThrow && result in VERDICT) {
        mood = result;
        status = charge.state === "waiting_for_cup_removal" ? `${VERDICT[result]}: lift the cup` : VERDICT[result];
    } else if (["starting", "zeroing", "waiting_for_cup_return"].includes(charge.state)) {
        mood = "not-ready";
    }
    $("panel").dataset.mood = mood;
    $("status").textContent = status;
}

function renderPurge() {
    if (purge === null) {
        return;
    }
    const running = purge.state === "running" || purge.state === "starting";
    // The unit's speed, not this page's: a purge set from the screen or another page shows here too (echo)
    if (running && typeof purge.speed_rps === "number") {
        purgeSpeed = purge.speed_rps;
    }
    $("purge-keys").hidden = !running;
    $("purge-start").hidden = running;
    $("purge-stop").hidden = !running;
    $("speed").textContent = purgeSpeed.toFixed(1);
    $("purge-status").textContent = purge.state === "fault" ? "Fault" : (running ? "Running" : "Ready");
    $("purge-panel").dataset.mood = purge.state === "fault" ? "over" : "normal";
    if (!running && purgeTimer !== null) {
        clearInterval(purgeTimer);
        purgeTimer = null;
        purgeSpeed = 0;
    }
}

// The switch at the Trickler's top: the job that runs is the view shown, and the other cannot be chosen
let view = "charge";

function show(wanted) {
    view = wanted;
    for (const name of ["charge", "purge"]) {
        $(`view-${name}`).hidden = name !== view;
        $(`seg-${name}`).classList.toggle("on", name === view);
        $(`seg-${name}`).setAttribute("aria-selected", String(name === view));
    }
}

function renderSwitch() {
    const charging = charge !== null && charge.state !== "idle" && charge.state !== "fault";
    const purging = purge !== null && (purge.state === "running" || purge.state === "starting");
    $("seg-purge").disabled = charging;
    $("seg-charge").disabled = purging;
    if (charging && view !== "charge") {
        show("charge");
    } else if (purging && view !== "purge") {
        show("purge");
    }
}

async function poll() {
    const [c, p] = await Promise.all([api("GET", "/api/v1/charge"), api("GET", "/api/v1/cleanup")]);
    if (c.ok) {
        charge = c.data;
        renderCharge();
    }
    if (p.ok) {
        purge = p.data;
        renderPurge();
    }
    renderSwitch();
}

function startPolling() {
    if (pollTimer === null) {
        poll();
        pollTimer = setInterval(poll, POLL_MS);
    }
}

function stopPolling() {
    clearInterval(pollTimer);
    pollTimer = null;
}

function askCup() {
    return new Promise((resolve) => {
        const dialog = $("cup");
        dialog.returnValue = "";
        dialog.onclose = () => resolve(dialog.returnValue === "yes");
        dialog.showModal();
    });
}

async function startCharge() {
    const target = Number($("target").value);
    if (!(target > 0)) {
        say("Give a target first.");
        return;
    }
    if (!await askCup()) {
        return;
    }
    const reply = await api("POST", "/api/v1/charge/start", {target, powder_profile: Number($("powder").value)});
    reply.ok ? say("") : refusal(reply, "Not started");
    poll();
}

async function stopCharge() {
    const reply = await api("POST", "/api/v1/charge/stop", {});
    reply.ok ? say("") : refusal(reply, "Not stopped");
    poll();
}

async function choosePowder() {
    const reply = await api("PATCH", "/api/v1/charge", {powder_profile: Number($("powder").value)});
    reply.ok ? say("") : refusal(reply, "Powder not changed");
    poll();
}

function stepTarget(direction) {
    const step = Math.pow(10, -decimals) * 10;
    const value = Math.max(Number($("target").value || 0) + direction * step, step);
    $("target").value = value.toFixed(decimals);
}

async function sendPurgeSpeed() {
    const reply = await api("PATCH", "/api/v1/cleanup", {speed_rps: purgeSpeed, time_s: 5});
    if (!reply.ok) {
        refusal(reply, "Speed not set");
        purgeSpeed = 0;
    }
    renderPurge();
}

async function startPurge() {
    if (!await askCup()) {
        return;
    }
    const reply = await api("POST", "/api/v1/cleanup/start", {});
    if (!reply.ok) {
        refusal(reply, "Purge not started");
        return;
    }
    say("");
    purgeSpeed = 0;
    if (purgeTimer === null) {
        purgeTimer = setInterval(() => purgeSpeed !== 0 && sendPurgeSpeed(), PURGE_RESEND_MS);
    }
    poll();
}

function stepPurge(direction) {
    purgeSpeed = Math.max(-PURGE_MAX, Math.min(PURGE_MAX, purgeSpeed + direction * PURGE_STEP));
    sendPurgeSpeed();
}

async function stopPurge() {
    const reply = await api("POST", "/api/v1/cleanup/stop", {});
    reply.ok ? say("") : refusal(reply, "Purge not stopped");
    poll();
}

async function zero() {
    const reply = await api("POST", "/api/v1/scale/zero", {});
    reply.ok ? say("Zeroed.") : refusal(reply, "Not zeroed");
}

// Keep this screen awake: on unless this browser was told otherwise, its switch on the Unit's page (rule 14: a
// preference set once is not on a job's view). Where the Wake Lock is not given (plain HTTP), gate 3 adds a fallback
function awakeWanted() {
    try {
        return localStorage.getItem("awake") !== "0";
    } catch (error) {
        return true;    // a browser with no storage: on, as the default
    }
}

async function keepAwake() {
    if (awakeWanted() && "wakeLock" in navigator && document.visibilityState === "visible") {
        wakeLock = await navigator.wakeLock.request("screen").catch(() => null);
    }
}

async function load() {
    const [powders, settings, network] = await Promise.all([api("GET", "/api/v1/powder_profiles"),
                                                            api("GET", "/api/v1/settings/charge"),
                                                            api("GET", "/api/v1/network")]);
    if (network.ok && network.data.hostname) {
        $("unit-name").textContent = network.data.hostname;
    }
    if (settings.ok && settings.data.decimal_places) {
        decimals = settings.data.decimal_places;
    }
    if (powders.ok) {
        $("powder").replaceChildren(...powders.data.profiles.map((profile) => new Option(profile.name, profile.id)));
    }
    $("start").onclick = startCharge;
    $("stop").onclick = stopCharge;
    $("powder").onchange = choosePowder;
    $("zero").onclick = zero;
    $("purge-start").onclick = startPurge;
    $("purge-stop").onclick = stopPurge;
    $("seg-charge").onclick = () => show("charge");
    $("seg-purge").onclick = () => show("purge");
    document.querySelectorAll("[data-step]").forEach((b) => b.onclick = () => stepTarget(Number(b.dataset.step)));
    document.querySelectorAll("[data-speed]").forEach((b) => b.onclick = () => stepPurge(Number(b.dataset.speed)));
    // A hidden tab reads nothing: the phone asleep, another app (the stream's life cycle in gate 2 does the same)
    document.addEventListener("visibilitychange", () => {
        document.visibilityState === "visible" ? (startPolling(), keepAwake()) : stopPolling();
    });
    startPolling();
    keepAwake();
}

load();
