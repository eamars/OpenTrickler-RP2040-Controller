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
    link.textContent = reached ? "Connected" : "Not reached";
    link.className = reached ? "badge badge-success" : "badge badge-error";
}

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
    $("bar").value = charge.target && charge.weight ? Math.min(charge.weight / charge.target, 1) : 0;
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
        step.classList.toggle("step-primary", reached >= 0 && index <= reached);
    });

    const verdict = {good: ["Good", "badge-success"], under: ["Under", "badge-warning"],
                     over: ["Over", "badge-error"]}[charge.last && charge.last.result];
    $("verdict").textContent = verdict ? `Throw ${charge.last.throw}: ${verdict[0]}` : "No throw yet";
    $("verdict").className = verdict ? `badge ${verdict[1]}` : "badge";
}

function renderPurge() {
    if (purge === null) {
        return;
    }
    const running = purge.state === "running" || purge.state === "starting";
    $("purge-idle").hidden = running;
    $("purge-running").hidden = !running;
    $("speed").textContent = purgeSpeed.toFixed(1);
    $("purge-start").disabled = charge !== null && charge.state !== "idle";
    if (!running && purgeTimer !== null) {
        clearInterval(purgeTimer);
        purgeTimer = null;
        purgeSpeed = 0;
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

async function keepAwake() {
    let wanted = $("awake").checked;
    try {
        localStorage.setItem("awake", wanted ? "1" : "0");
    } catch (error) {
        // a browser with no storage: the switch holds for this visit only
    }
    if (wanted && "wakeLock" in navigator && document.visibilityState === "visible") {
        wakeLock = await navigator.wakeLock.request("screen").catch(() => null);
    } else if (!wanted && wakeLock !== null) {
        wakeLock.release();
        wakeLock = null;
    }
}

async function load() {
    try {
        $("awake").checked = localStorage.getItem("awake") !== "0";
    } catch (error) {
        $("awake").checked = true;
    }
    const [powders, settings] = await Promise.all([api("GET", "/api/v1/powder_profiles"),
                                                   api("GET", "/api/v1/settings/charge")]);
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
    $("awake").onchange = keepAwake;
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
