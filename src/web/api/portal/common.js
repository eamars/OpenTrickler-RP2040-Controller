// The portal's shared part (ADR-008): the API, messages, the link to the unit, the pages and their router, the one
// confirmation dialog, files to and from the person. The pages' own parts follow it (trickler.js, form.js,
// settings.js, powders.js, unit.js), joined into one file by scripts/web_build.py. The same script runs against the
// stand-in (tests/web_test.py) and the unit. The unit's changes come by its event stream (stream.js).
"use strict";

const $ = (id) => document.getElementById(id);

// One call to the unit; the link is shown lost when the unit does not answer (web-ui-design, rule 9)
// limitMs: a read given up after that long, as one not answered (a unit gone without a word leaves it hanging)
// A change refused for want of the unit's PIN (ADR-016, gate 3) asks for it, keeps it, and is sent again: asked again
// after a wrong one, until it is right, the person cancels, or the unit refuses every change for a while (429)
async function api(method, path, body, limitMs) {
    let reply = await apiOnce(method, path, body, limitMs);
    for (;;) {
        const code = reply.data && reply.data.error ? reply.data.error.code : null;
        if (reply.status !== 401 || (code !== "pin_required" && code !== "pin_wrong")) {
            return reply;
        }
        const digits = await askPin(code === "pin_wrong");
        if (!digits) {
            return reply;
        }
        setPinCookie(digits);
        reply = await apiOnce(method, path, body, limitMs);
    }
}

async function apiOnce(method, path, body, limitMs) {
    try {
        const binary = body instanceof Blob || body instanceof ArrayBuffer;
        const response = await fetch(path, {
            method,
            headers: body === undefined ? {} : {"Content-Type": binary ? "application/octet-stream" : "application/json"},
            body: body === undefined ? undefined : (binary ? body : JSON.stringify(body)),
            signal: limitMs ? AbortSignal.timeout(limitMs) : undefined,
        });
        const data = await response.json().catch(() => null);
        setLink(true);
        return {ok: response.ok, status: response.status, data};
    } catch (error) {
        setLink(false);
        return {ok: false, status: 0, data: null};
    }
}

function setLink(reached) {
    // A restart this page follows is not a lost unit (update.js), nor a hotspot the set-up has just left (setup.js);
    // through the set-up its own lost view says it, with what to do (setup.js, setupContact)
    // An update this page follows, once installing, restarts the unit: any read may be the first to find it gone
    const restarting = follow.on && ["ready", "installing"].includes(follow.last);
    $("link").hidden = reached || follow.away || restarting || setupLeft || pageShown === "setup";
}

// A line over the page, the unit's own words when it refuses; gone at the next action or a tap
function say(text, kind) {
    const message = $("message");
    message.textContent = text;
    message.className = kind === "good" ? "message good" : "message";
}

function refusalText(reply) {
    return reply.data && reply.data.error ? reply.data.error.message : "the unit was not reached";
}

// features: 27
function refusal(reply, what) {
    say(`${what}: ${refusalText(reply)}`);
}

// The one confirmation, for what cannot be undone (rule 8): what is lost said, buttons named by their outcome, no
// default, the safe one first. Its action red only where it loses something (a factory reset): an install or a restart
// moves the unit on, in the colour of the action that does (rule 5)
function confirmAction(title, text, act, keep, loses = false) {
    return new Promise((resolve) => {
        const dialog = $("confirm");
        $("confirm-title").textContent = title;
        $("confirm-text").textContent = text;
        $("confirm-act").textContent = act;
        $("confirm-act").className = loses ? "action halt" : "action go";
        $("confirm-keep").textContent = keep;
        dialog.returnValue = "";
        dialog.onclose = () => resolve(dialog.returnValue === "act");
        dialog.showModal();
        $("confirm-keep").focus();
    });
}

// The web's PIN (ADR-016, gate 3), kept by this browser as the cookie the unit reads, pin=<digits>, sent to the
// unit's own pages alone (SameSite=Strict), until a change is refused again (the unit restarted: a new PIN)
function setPinCookie(digits) {
    document.cookie = `pin=${digits}; path=/; SameSite=Strict`;
}

// Asked when a change is refused for want of it, in the one confirmation with its field shown: the digits, or null
// when the person cancels. Enter in the field enters it (the form's first button is the safe one, on the left)
async function askPin(wrong) {
    const field = $("confirm-field");
    field.value = "";
    field.hidden = false;
    field.onkeydown = (event) => {
        if (event.key === "Enter") {
            event.preventDefault();
            $("confirm").close("act");
        }
    };
    const asked = confirmAction("Enter the unit's PIN", wrong
        ? "That is not the PIN on the unit's Wi-Fi page: it is new each time the unit starts. Try again."
        : "The unit asks for the PIN on its Wi-Fi page before it takes a change.", "Enter", "Cancel");
    field.focus();
    const entered = await asked;
    field.hidden = true;
    return entered && field.value.trim() ? field.value.trim() : null;
}

// The cup's question before a charge or a purge: a safety step, in the screen's words (rule 8)
function askCup() {
    return new Promise((resolve) => {
        const dialog = $("cup");
        dialog.returnValue = "";
        dialog.onclose = () => resolve(dialog.returnValue === "yes");
        dialog.showModal();
    });
}

function download(name, data) {
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([JSON.stringify(data, null, 1)], {type: "application/json"}));
    link.download = name;
    link.click();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function chooseFile(accept) {
    return new Promise((resolve) => {
        const input = document.createElement("input");
        input.type = "file";
        input.accept = accept;
        input.onchange = () => resolve(input.files[0] || null);
        input.click();
    });
}

function element(tag, attributes, ...children) {
    const made = document.createElement(tag);
    for (const [name, value] of Object.entries(attributes || {})) {
        if (name.startsWith("on")) {
            made[name] = value;
        } else if (value !== false && value !== null && value !== undefined) {
            made.setAttribute(name, value === true ? "" : value);
        }
    }
    made.append(...children.filter((child) => child !== null && child !== undefined));
    return made;
}

// The pages: each has enter(route) and leave(); the address says which page and what in it (#settings/charge)
const PAGES = {};
let pageShown = null;

// A window wide enough for a list beside the part it opens (the split pages' layout in plain.css)
const SPLIT = window.matchMedia("(min-width: 1000px)");

// features: 9
function route() {
    // The old mirror's address opens the Unit page, where the mirror is (features: 5)
    const first = location.pathname === "/display_mirror" ? "unit" : "trickler";
    const [name, ...rest] = (location.hash.slice(1) || first).split("/");
    const page = PAGES[name] ? name : "trickler";
    if (pageShown !== page) {
        if (pageShown) {
            PAGES[pageShown].leave();
            $(`page-${pageShown}`).hidden = true;
        }
        pageShown = page;
        document.body.dataset.page = page;
        $(`page-${page}`).hidden = false;
        document.querySelectorAll(".nav a").forEach((a) => a.classList.toggle("on", a.dataset.page === page));
    }
    say("");
    PAGES[page].enter(rest.join("/"));
    markOffer();
    drawJobs();     // the job's strip, shown on every page but the job's own (update.js)
}

// A release found is marked on the Unit's destination, from every page, as the screen offers it by a row of its main
// menu: never a pop-up (ADR-004). Read at each page shown, and at the poll's `found` event (update.js)
async function markOffer() {
    const reply = await api("GET", "/api/v1/update");
    if (reply.ok) {
        showOfferMark(reply.data.found);
    }
}

function showOfferMark(found) {
    const unit = document.querySelector('.nav a[data-page="unit"]');
    unit.classList.toggle("offer-mark", found !== null);
    unit.classList.toggle("critical", found !== null && found.importance === "critical");
    unit.setAttribute("aria-label", found === null ? "Unit" : `Unit: version ${found.version} is available`);
}

// Keep this screen awake: on unless this browser was told otherwise (its switch on the Unit's page). Where the Wake
// Lock is not given (plain HTTP), the README's muted video comes with the phones' checks at gate 4's start: only a
// real phone shows whether it works and what it costs the battery
// features: 14
function awakeWanted() {
    try {
        return localStorage.getItem("awake") !== "0";
    } catch (error) {
        return true;    // a browser with no storage: on, as the default
    }
}

function setAwakeWanted(wanted) {
    try {
        localStorage.setItem("awake", wanted ? "1" : "0");
    } catch (error) {
        // no storage: the switch holds for this visit only
    }
    keepAwake();
}

let wakeLock = null;

async function keepAwake() {
    if (awakeWanted() && "wakeLock" in navigator && document.visibilityState === "visible") {
        wakeLock = await navigator.wakeLock.request("screen").catch(() => null);
    } else if (!awakeWanted() && wakeLock !== null) {
        wakeLock.release();
        wakeLock = null;
    }
}
