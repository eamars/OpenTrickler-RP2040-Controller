// The start: the page the address names, the next when it changes; a hidden tab reads nothing (the phone asleep,
// another app), and reads again when shown (the event stream's life cycle in gate 2 does the same)
// A new address is a new page: it starts at its top, not where the last one was left
window.addEventListener("hashchange", () => {
    window.scrollTo(0, 0);
    route();
});
// A list and its part side by side, or one at a time: drawn again when the window crosses the width
SPLIT.addEventListener("change", () => {
    if (pageShown === "settings" || pageShown === "powders") {
        route();
    }
});
document.addEventListener("visibilitychange", () => {
    if (pageShown === null) {
        return;
    }
    if (document.visibilityState === "visible") {
        keepAwake();
        route();
    } else {
        PAGES[pageShown].leave();
    }
});
$("message").onclick = () => say("");
route();
keepAwake();
watchJobs();
setInterval(watchJobs, WATCH_MS);
