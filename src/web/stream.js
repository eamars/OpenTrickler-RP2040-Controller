// The unit to the page (ADR-008, gate 3): one event stream for the whole page, its life cycle as the README's table has
// it. A page listens with onUnit(kind, handler): "state" (the job, the charge, the purge, as their GETs), "event" (each
// event of the log), "afresh" (the stream started again with something missed: read what is shown again)
const STREAM_PATH = "/api/v1/events/stream";
const STREAM_SILENT_MS = 20000;     // the unit sends its state again after 15 s quiet: nothing for 20 s is a unit gone
const STREAM_RETRY_MS = 5000;       // after a refusal (503 past three streams), as its Retry-After says
const STREAM_CHECK_MS = 2000;

const streamHandlers = {state: [], event: [], afresh: []};
let streamSource = null;
let streamHeard = 0;
let streamRetry = null;

function onUnit(kind, handler) {
    streamHandlers[kind].push(handler);
}

function streamTell(kind, data) {
    for (const handler of streamHandlers[kind]) {
        handler(data);
    }
}

function streamOpen() {
    if (streamSource !== null || document.visibilityState !== "visible") {
        return;
    }
    clearTimeout(streamRetry);
    streamRetry = null;
    streamHeard = Date.now();
    const source = new EventSource(STREAM_PATH);
    streamSource = source;
    source.addEventListener("unit", (message) => {
        streamHeard = Date.now();
        if (!JSON.parse(message.data).resumed) {
            streamTell("afresh", null);
        }
    });
    source.addEventListener("state", (message) => {
        streamHeard = Date.now();
        streamTell("state", JSON.parse(message.data));
    });
    source.onmessage = (message) => {
        streamHeard = Date.now();
        streamTell("event", JSON.parse(message.data));
    };
    source.onerror = () => {
        // Dropped: the browser opens it again by itself, with its last id. Refused (an error status): it does not,
        // so the page does, after a while
        if (source.readyState === EventSource.CLOSED) {
            streamClose();
            streamRetry = setTimeout(streamOpen, STREAM_RETRY_MS);
        }
    };
}

function streamClose() {
    if (streamSource !== null) {
        streamSource.close();
        streamSource = null;
    }
}

// A unit that restarts or leaves its network sends no close: the stream is silent, and the page opens it again
setInterval(() => {
    if (streamSource !== null && Date.now() - streamHeard > STREAM_SILENT_MS) {
        streamClose();
        streamOpen();
    }
}, STREAM_CHECK_MS);

// A tab hidden closes it, so its slot on the unit is free before it goes stale; shown, it opens again
document.addEventListener("visibilitychange", () => {
    document.visibilityState === "visible" ? streamOpen() : streamClose();
});
