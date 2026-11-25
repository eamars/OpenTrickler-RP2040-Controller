// The Trickler (ADR-008): the charge and the purge, a switch between them, each view the height of the screen.
// features: 32
const PURGE_RESEND_MS = 2000;   // a purge's speed lasts 5 s on the unit: a page that goes leaves no motor turning
const PURGE_STEP = 0.5;
const PURGE_MAX = 5;

let charge = null;
let purge = null;
let decimals = 2;
let purgeSpeed = 0;
let purgeTimer = null;

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
    const share = charge.target && charge.weight ? Math.min(Math.max(charge.weight / charge.target, 0), 1) : 0;
    $("ring").style.strokeDashoffset = String(1000 * (1 - share));
    $("ring").style.visibility = share > 0 ? "visible" : "hidden";  // a round end at nought is drawn as a dot
    $("bar").style.width = `${share * 100}%`;
    if (document.activeElement !== $("target") && charge.target !== null) {
        $("target").value = Number(charge.target).toFixed(decimals);
    }
    if (document.activeElement !== $("powder")) {
        $("powder").value = String(charge.powder_profile);
    }
    // What to throw is locked while it is thrown: the field and its keys alike
    $("powder").disabled = running;
    $("target").disabled = running;
    document.querySelectorAll("[data-step]").forEach((key) => key.disabled = running);
    $("zero").disabled = running;   // the charge zeroes the pan itself; a zero mid-throw would spoil it
    $("start").hidden = running;
    $("stop").hidden = !running;

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
    // The time is the current charge's: shown while it runs and with its verdict, not a past one's while Ready
    const timed = (running || afterThrow) && typeof charge.elapsed_s === "number";
    $("elapsed").hidden = !timed;
    $("elapsed").textContent = timed ? `${charge.elapsed_s.toFixed(2)} s` : "";
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
    // The speed's keys either side of the speed, while a purge runs; the line on what a purge does, before it does
    document.querySelectorAll("[data-speed]").forEach((key) => key.hidden = !running);
    $("purge-hint").hidden = running;
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
        $(name === "charge" ? "panel" : "purge-panel").hidden = name !== view;
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

// features: 28
async function poll() {
    const [c, p] = await Promise.all([api("GET", "/api/v1/charge"), api("GET", "/api/v1/purge")]);
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

// The unit's state as it changes, from the event stream (gate 3), in place of a read every 500 ms: drawn while shown
let tricklerShown = false;
onUnit("state", (state) => {
    if (!tricklerShown) {
        return;
    }
    charge = state.charge;
    purge = state.purge;
    renderCharge();
    renderPurge();
    renderSwitch();
});

// Something missed (a restart, a long gap): read again what is shown
onUnit("afresh", () => tricklerShown && poll());

function startPolling() {
    tricklerShown = true;
    poll();
}

function stopPolling() {
    tricklerShown = false;
}

// features: 23 N5
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
    const reply = await api("PATCH", "/api/v1/purge", {speed_rps: purgeSpeed, time_s: 5});
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
    const reply = await api("POST", "/api/v1/purge/start", {});
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
    const reply = await api("POST", "/api/v1/purge/stop", {});
    reply.ok ? say("") : refusal(reply, "Purge not stopped");
    poll();
}

async function zero() {
    const reply = await api("POST", "/api/v1/scale/zero", {});
    reply.ok ? say("Zeroed.", "good") : refusal(reply, "Not zeroed");
}

// The page: its controls wired once, its polling only while it is shown
let tricklerWired = false;

PAGES.trickler = {
    async enter() {
        if (!tricklerWired) {
            tricklerWired = true;
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
        }
        // The powders' names and the charge's decimals again at each visit: either may have changed elsewhere
        const [powders, settings] = await Promise.all([api("GET", "/api/v1/powder_profiles"),
                                                       api("GET", "/api/v1/settings/charge")]);
        if (settings.ok && settings.data.decimal_places) {
            decimals = settings.data.decimal_places;
        }
        if (powders.ok) {
            const chosen = $("powder").value;
            $("powder").replaceChildren(...powders.data.profiles.map((p) => new Option(p.name, p.id)));
            $("powder").value = chosen;
        }
        startPolling();
    },
    leave() {
        stopPolling();
    },
};
