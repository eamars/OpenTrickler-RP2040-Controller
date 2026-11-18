// The start: the page the address names, the next when it changes; a hidden tab reads nothing (the phone asleep,
// another app), and reads again when shown (the event stream's life cycle in gate 2 does the same)
window.addEventListener("hashchange", route);
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
