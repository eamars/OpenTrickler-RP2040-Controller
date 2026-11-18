// The update and the restart (ADR-004, ADR-008): the offer; the job as the screen shows it, in its words; the unit
// followed through its restart; how it ended, in words (README, "The update's life cycle, read whole"). A job is
// watched from every page, and a page that is not the job's shows a strip with it and its Stop (web-ui-design,
// rule 9): the charge's and the purge's too. Read once a second for now; gate 2's event stream carries the same.

const WATCH_MS = 1000;
const BACK_WITHIN_S = 90;   // a unit not back by then is said so, where the unit does not give its own (gate 2)

/*
 * What this page follows across a restart, an update's or a plain one: what it was asked to become, what ran before,
 * the boot it began on, the job's state last seen, and whether the unit is away. A restart takes the page's
 * connection with it, so being away is expected then, not a fault: the "not reached" banner holds back (rule 9)
 */
const follow = {on: false, restartOnly: false, version: null, from: null, boot: null, last: null, away: false,
                awaySince: 0, within: BACK_WITHIN_S, trial: false, endText: null, endKind: null};
let sending = null;         // this page's own upload: the file's name and how much of it has gone
let watched = {activity: "none", update: null};
let updateKey = "";

// The job's view, from the unit's update and what this page follows; null when no job runs
function jobView(u) {
    if (sending !== null) {
        return {word: `Sending ${sending.name}`, share: sending.share, stop: false};
    }
    if (follow.away) {
        const late = (Date.now() - follow.awaySince) / 1000 > follow.within;
        const word = follow.restartOnly ? "Restarting" : `Restarting on ${follow.version || "the new firmware"}`;
        return {word, share: null, stop: false, late};
    }
    if (follow.trial) {
        return {word: `Starting ${follow.version}`, share: null, stop: false};
    }
    if (u === null || u.state === "idle") {
        return null;
    }
    const share = u.expected > 0 ? u.received / u.expected : null;
    const waiting = u.download ? "Connecting" : "Waiting for the image";
    // The screen's job titles, word for word (menu_nav.c, the update's job screen)
    switch (u.state) {
        case "starting":
            return {word: waiting, share: null, stop: true};
        case "receiving":
            return {word: u.expected === 0 ? waiting : (u.download ? `Downloading ${u.found ? u.found.version : ""}`.trim() : "Receiving"),
                    share, stop: true};
        case "ready":
            return {word: u.download ? "Checking" : "Ready to install", share: null, stop: true};
        case "installing":
            return {word: "Installing", share: null, stop: false};
        default:
            return {word: "Ending", share: null, stop: false};
    }
}

// Why an image was refused, in plain words by its code (README, "What the page has to do"); the unit's own line for
// a code not known here
const REFUSED = {
    hash_mismatch: "the download did not match its release; check again and install again",
    wrong_board: "the file is for another board",
    wrong_chip: "the file is for another board",
    wrong_slot: "the file is for this board's other slot: choose the one this unit takes",
    not_newer: "the file is not newer than the firmware running",
    not_an_image: "the file is not firmware",
    not_an_update: "the file is a full install, not an update",
    too_large: "the file is larger than the unit has room for",
};

function refusedText(last) {
    return REFUSED[last.code] || last.message;
}

// How the last update ended, as the unit keeps it until its next restart: a past fact, said as one
function lastText(last) {
    if (last === null || last === undefined) {
        return follow.endText;      // what this page saw itself, where the unit could not say (a power cut in a trial)
    }
    switch (last.result) {
        case "confirmed":
            return `Last update: ${last.version} installed.`;
        case "back":
            return `Last update: ${last.version} was not kept; the unit went back to the version it had.`;
        case "refused":
            return `Last update: not installed: ${refusedText(last)}.`;
        case "time":
            return "Last update: stopped, the image did not come in time. Nothing installed.";
        default:
            return "Last update: stopped. Nothing installed.";
    }
}

// The end, said once when this page saw the job through: what the unit says, or what the page saw
function endOf(u, restarted) {
    const last = u.last;
    if (last && last.result === "confirmed") {
        return ["good", `Updated to ${u.running.version}.`];
    }
    if (last && last.result === "back") {
        return ["bad", `${last.version} was not kept: the unit went back to ${u.running.version}.`];
    }
    if (last && last.result === "refused") {
        return ["bad", `Not installed: ${refusedText(last)}.`];
    }
    if (last && (last.result === "stopped" || last.result === "time")) {
        return ["bad", lastText(last).replace("Last update: s", "S")];
    }
    if (restarted) {
        // Back on what it ran before, and the unit does not know it tried (gap 2): a power cut in the trial
        return ["bad", `${follow.version || "The new firmware"} was not kept: the unit is on ${u.running.version}. The power may have been cut.`];
    }
    return ["bad", "The update ended. Nothing installed."];
}

function followStart(u, index) {
    Object.assign(follow, {on: true, restartOnly: false, version: null,
                           from: u.running.version, boot: index.boot, last: u.state, away: false, trial: false,
                           within: BACK_WITHIN_S, endText: null, endKind: null});
}

// A restart asked by this page (the Unit's Restart): followed as an update's is, to its new boot (features: 68)
function followRestart(boot) {
    Object.assign(follow, {on: true, restartOnly: true, version: null, from: null, boot, last: "installing",
                           away: false, trial: false, within: BACK_WITHIN_S, endText: null, endKind: null});
}

function followUpdate(u, index) {
    watched.update = u;
    if (!follow.on && u.state !== "idle") {
        followStart(u, index);  // another page, the screen or an agent began it: every client follows (echo)
    }
    if (!follow.on) {
        return;
    }
    if (u.state !== "idle") {
        follow.last = u.state;
        if (u.download && u.found) {
            follow.version = u.found.version;   // the release the job fetches; a file's version is not known before
        }
        if (u.state === "installing") {
            follow.within = u.back_within_s || BACK_WITHIN_S;
        }
    }
    if (index.boot !== follow.boot) {
        follow.away = false;
        if (follow.restartOnly) {
            follow.on = false;
            say("Restarted.", "good");
            return;
        }
        if (u.running.trial) {
            Object.assign(follow, {trial: true, version: u.running.version});
            return;
        }
        finish(u, true);
    } else if (u.state === "idle" && !follow.restartOnly) {
        finish(u, false);
    }
}

function finish(u, restarted) {
    const [kind, text] = endOf(u, restarted);
    const updated = restarted && u.running.version !== follow.from;
    Object.assign(follow, {on: false, trial: false, endText: u.last ? null : text, endKind: kind});
    if (updated) {
        // The new firmware serves its own pages: this one is the old firmware's, so it is loaded again, on the Unit
        // page, where the unit's `last` says how it ended
        location.hash = "#unit";
        location.reload();
        return;
    }
    // Said once: on the Unit page its card says it (the last update's line), elsewhere a message does
    if (pageShown !== "unit") {
        say(text, kind === "good" ? "good" : "");
    }
}

// The unit away while followed through a restart: expected, so the banner holds back and the job says it
function goneAway() {
    if (follow.on && ["ready", "installing"].includes(follow.last) && !follow.away) {
        Object.assign(follow, {away: true, awaySince: Date.now()});
    }
    setLink(follow.away);
}

// The job from the stream's state, an update's stages from its events (gate 3); read here only while it matters
onUnit("state", (state) => {
    if (watched.activity !== state.activity) {
        watched.activity = state.activity;
        watchJobs();
    }
    drawJobs();
});
onUnit("event", (event) => {
    if (event.source === "update") {
        watchJobs();
        if (event.kind === "found") {
            markOffer();    // the offer's mark on the Unit's destination, on every page
        }
    }
});

onUnit("afresh", () => watchJobs());

async function watchJobs() {
    if (document.visibilityState !== "visible" || pageShown === null) {
        return;
    }
    if (!(watched.activity === "update" || follow.on || pageShown === "unit")) {
        drawJobs();
        return;     // the stream says the rest
    }
    const system = await api("GET", "/api/v1/system");
    if (!system.ok) {
        goneAway();
        drawJobs();
        return;
    }
    watched.activity = system.data.activity;
    if (watched.activity === "update" || follow.on || pageShown === "unit") {
        const [update, index] = await Promise.all([api("GET", "/api/v1/update"), api("GET", "/api/v1")]);
        if (update.ok && index.ok) {
            followUpdate(update.data, index.data);
            showOfferMark(update.data.found);
        }
    }
    drawJobs();
}

// --- The job's strip, on every page that is not the job's

const STRIP_JOBS = {
    charge: {word: "Charging", page: "trickler", stop: "/api/v1/charge/stop"},
    purge: {word: "Purging", page: "trickler", stop: "/api/v1/purge/stop"},
};

function drawJobs() {
    const view = jobView(watched.update);
    let strip = null;
    if (view !== null) {
        strip = {word: view.share === null ? view.word : `${view.word} · ${Math.floor(view.share * 100)} %`, page: "unit",
                 stop: view.stop ? "/api/v1/update/stop" : null};
    } else if (STRIP_JOBS[watched.activity]) {
        strip = STRIP_JOBS[watched.activity];
    }
    const shown = strip !== null && strip.page !== pageShown;
    $("strip").hidden = !shown;
    document.body.classList.toggle("strip-on", shown);
    if (shown) {
        $("strip-word").textContent = strip.word;
        $("strip-go").href = `#${strip.page}`;
        $("strip-stop").hidden = strip.stop === null;
        $("strip-stop").onclick = async () => {
            const reply = await api("POST", strip.stop, {});
            reply.ok ? watchJobs() : refusal(reply, "Not stopped");
        };
    }
    if (pageShown === "unit") {
        redrawUpdateCard();
    }
}

// --- The Unit page's Update card: the offer and its actions, or the job while one runs (features: N1 N2)

function updateBody(u) {
    const body = [element("dl", {class: "facts"}, fact("Firmware", u.running.version))];
    const view = jobView(u);
    const last = view === null ? lastText(u.last) : null;
    if (last) {
        body.push(element("p", {class: "hint last"}, last));    // what happened to what runs, before what is offered
    }
    if (view !== null) {
        const percent = view.share === null ? "" : String(Math.floor(view.share * 100));
        const stop = element("button", {class: "action halt", type: "button", onclick: stopUpdate}, "Stop");
        stop.style.visibility = view.stop ? "visible" : "hidden";      // nothing jumps when Stop goes (rule 14)
        body.push(element("div", {class: "job-view"},
                          element("p", {class: "job-word"}, view.word),
                          element("p", {class: "job-figure"}, element("span", {}, percent || " "),
                                  element("span", {class: "job-unit"}, percent ? "%" : "")),
                          element("div", {class: "bar", style: view.share === null ? "visibility: hidden" : ""},
                                  element("i", {style: `width: ${view.share === null ? 0 : view.share * 100}%`})),
                          element("p", {class: "job-note"}, view.late ? "Not back yet. Still trying: keep the power on."
                                                                     : "Keep the power on."),
                          stop));
        return body;
    }
    if (u.found) {
        const critical = u.found.importance === "critical";
        body.push(element("div", {class: critical ? "offer critical" : "offer"},
                          element("p", {class: "offer-title"}, `Version ${u.found.version} is available`,
                                  critical ? element("span", {class: "tag critical"}, "Important") : null),
                          u.found.note ? element("p", {}, u.found.note) : null,
                          u.found.bench ? element("p", {class: "hint"}, `From the bench host ${u.found.bench}`) : null,
                          element("button", {class: "action go", type: "button", onclick: installFound}, "Install")));
    } else {
        const poll = u.poll || {};
        const said = poll.state === "failed" ? `The last check failed: ${poll.error}.` :
            (poll.state === "checking" ? "Checking…" : "No newer firmware found.");
        body.push(element("p", {class: "hint"}, said));
    }
    body.push(element("div", {class: "button-row"},
                      element("button", {class: "ghost", type: "button", onclick: checkUpdate}, "Check now"),
                      element("button", {class: "ghost", type: "button", onclick: () => installFile(u)}, "Install from a file"),
                      element("a", {class: "ghost link-button", href: "#settings/update"}, "Where updates come from")));
    return body;
}

function drawUpdateCard(update) {
    const section = element("section", {class: "card", id: "update-card"}, element("h2", {}, "Update"));
    if (!update.ok) {
        section.append(element("p", {class: "hint"}, `Not read: ${refusalText(update)}`));
        return section;
    }
    watched.update = update.data;
    updateKey = "";
    section.append(...updateBody(update.data));
    return section;
}

// Drawn again only when what it says changed: a button under a finger is not replaced every second
function redrawUpdateCard() {
    const section = $("update-card");
    const u = watched.update;
    if (section === null || u === null) {
        return;
    }
    const key = JSON.stringify([jobView(u), u.found, u.poll, u.last, u.running.version, follow.endText]);
    if (key === updateKey) {
        return;
    }
    updateKey = key;
    section.replaceChildren(element("h2", {}, "Update"), ...updateBody(u));
}

// features: N3
async function checkUpdate() {
    // The card says "Checking..." and then what came of it: a message would say it twice
    const reply = await api("POST", "/api/v1/update/check", {});
    reply.ok ? watchJobs() : refusal(reply, "Not checked");
}

async function stopUpdate() {
    const reply = await api("POST", "/api/v1/update/stop", {});
    reply.ok ? watchJobs() : refusal(reply, "Not stopped");
}

async function installFound() {
    const sure = await confirmAction("Install the update?",
                                     "The unit fetches it, checks it and restarts on it. Keep the power on until it is back. If the new firmware does not start, it goes back to the one it has now.",
                                     "Install", "Not now");
    if (!sure) {
        return;
    }
    const reply = await api("POST", "/api/v1/update/start", {download: true});
    reply.ok ? watchJobs() : refusal(reply, "Not started");
}

// The image sent as the unit takes it, with how much has gone: a fetch() gives no upload's progress
function sendImage(file) {
    return new Promise((resolve) => {
        const request = new XMLHttpRequest();
        request.open("PUT", "/api/v1/update/image");
        request.setRequestHeader("Content-Type", "application/octet-stream");
        request.upload.onprogress = (event) => {
            if (event.lengthComputable) {
                sending.share = event.loaded / event.total;
                drawJobs();
            }
        };
        request.onload = () => {
            let data = null;
            try {
                data = JSON.parse(request.responseText);
            } catch (error) {
                data = null;    // a reply that is not JSON: the status says enough
            }
            resolve({ok: request.status >= 200 && request.status < 300, status: request.status, data});
        };
        request.onerror = () => resolve({ok: false, status: 0, data: null});
        request.send(file);
    });
}

// Whether the update's job is receiving within a few seconds of its start
async function receiving() {
    for (let tries = 0; tries < 20; tries++) {
        const reply = await api("GET", "/api/v1/update");
        if (reply.ok && reply.data.state === "receiving") {
            return true;
        }
        if (reply.ok && reply.data.state === "idle") {
            return false;       // ended before it took anything
        }
        await new Promise((resolve) => setTimeout(resolve, 250));
    }
    return false;
}

async function installFile(u) {
    say(`Choose the file ending in ${u.takes}_update.bin`, "good");
    const file = await chooseFile(".bin,application/octet-stream");
    say("");
    if (file === null) {
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
    await watchJobs();      // followed from its start: a refusal at once is still said
    // The unit takes an image only once its job is receiving: until the menu begins it, it is starting
    if (!await receiving()) {
        say("Not sent: the unit did not get ready for the file.");
        await api("POST", "/api/v1/update/stop", {});
        watchJobs();
        return;
    }
    sending = {name: file.name, share: 0};
    drawJobs();
    const sent = await sendImage(file);
    sending = null;
    if (!sent.ok) {
        // A refused image (400, 413) ends the job, and the unit's `last` says why, in words by the watch. Anything
        // else (the connection lost, another upload) leaves the job waiting for an image that will not come: stopped
        if (sent.status !== 400 && sent.status !== 413) {
            refusal(sent, "Not sent");
            await api("POST", "/api/v1/update/stop", {});
        }
        watchJobs();
        return;
    }
    const installed = await api("POST", "/api/v1/update/install", {});
    installed.ok ? watchJobs() : refusal(installed, "Not installed");
}
