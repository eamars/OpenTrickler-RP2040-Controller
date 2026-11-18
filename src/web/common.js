// The portal's shared part (ADR-008): the API, messages, the link to the unit, the pages and their router, the one
// confirmation dialog, files to and from the person. The pages' own parts follow it (trickler.js, form.js,
// settings.js, powders.js, unit.js), joined into one file by scripts/web_build.py. The same script runs against the
// stand-in (tests/web_test.py) and the unit. Polling for now; the event stream replaces it in gate 2.
"use strict";

const $ = (id) => document.getElementById(id);

// One call to the unit; the link is shown lost when the unit does not answer (web-ui-design, rule 9)
async function api(method, path, body) {
    try {
        const binary = body instanceof Blob || body instanceof ArrayBuffer;
        const response = await fetch(path, {
            method,
            headers: body === undefined ? {} : {"Content-Type": binary ? "application/octet-stream" : "application/json"},
            body: body === undefined ? undefined : (binary ? body : JSON.stringify(body)),
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
    $("link").hidden = reached;
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

function refusal(reply, what) {
    say(`${what}: ${refusalText(reply)}`);
}

// The one confirmation, for what cannot be undone (rule 8): what is lost said, buttons named by their outcome, no
// default, the safe one first
function confirmAction(title, text, act, keep) {
    return new Promise((resolve) => {
        const dialog = $("confirm");
        $("confirm-title").textContent = title;
        $("confirm-text").textContent = text;
        $("confirm-act").textContent = act;
        $("confirm-keep").textContent = keep;
        dialog.returnValue = "";
        dialog.onclose = () => resolve(dialog.returnValue === "act");
        dialog.showModal();
        $("confirm-keep").focus();
    });
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

function route() {
    const [name, ...rest] = (location.hash.slice(1) || "trickler").split("/");
    const page = PAGES[name] ? name : "trickler";
    if (pageShown !== page) {
        if (pageShown) {
            PAGES[pageShown].leave();
            $(`page-${pageShown}`).hidden = true;
        }
        pageShown = page;
        $(`page-${page}`).hidden = false;
        document.querySelectorAll(".nav a").forEach((a) => a.classList.toggle("on", a.dataset.page === page));
    }
    say("");
    PAGES[page].enter(rest.join("/"));
    markOffer();
}

// A release found is marked on the Unit's destination, from every page, as the screen offers it by a row of its main
// menu: never a pop-up (ADR-004). Read at each page shown; the event stream carries it in gate 2
async function markOffer() {
    const reply = await api("GET", "/api/v1/update");
    const found = reply.ok ? reply.data.found : null;
    const unit = document.querySelector('.nav a[data-page="unit"]');
    unit.classList.toggle("offer-mark", found !== null);
    unit.classList.toggle("critical", found !== null && found.importance === "critical");
    unit.setAttribute("aria-label", found === null ? "Unit" : `Unit: version ${found.version} is available`);
}

// Keep this screen awake: on unless this browser was told otherwise (its switch on the Unit's page). Where the Wake
// Lock is not given (plain HTTP), gate 3 adds a fallback
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
