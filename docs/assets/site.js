// Keeps the download buttons on the newest release and suggests the visitor's language.
(function () {
    "use strict";

    var REPO = "dionatansimioni/KamLynk";

    // The HTML already links to a release, so this only matters after a new one is published.
    fetch("https://api.github.com/repos/" + REPO + "/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
        .then(function (r) { return r.ok ? r.json() : null; })
        .then(function (release) {
            if (!release || !release.assets) return;
            var appImage = release.assets.filter(function (a) { return /x86_64\.AppImage$/.test(a.name); })[0];
            if (!appImage) return;
            var version = String(release.tag_name || "").replace(/^v/, "");
            document.querySelectorAll("[data-download]").forEach(function (a) { a.href = appImage.browser_download_url; });
            document.querySelectorAll("[data-version]").forEach(function (el) { el.textContent = version; });
            document.querySelectorAll("[data-size]").forEach(function (el) {
                el.textContent = Math.round(appImage.size / 1048576) + " MB";
            });
        })
        .catch(function () { /* offline or rate-limited: keep the static links */ });

    // Offer the other language once, without redirecting (search engines see both pages).
    var page = document.documentElement.lang.toLowerCase().indexOf("pt") === 0 ? "pt" : "en";
    var wanted = (navigator.languages && navigator.languages[0] || navigator.language || "").toLowerCase()
        .indexOf("pt") === 0 ? "pt" : "en";
    var hint = document.getElementById("lang-hint");
    var dismissed = false;
    try { dismissed = localStorage.getItem("kamlynk-lang-hint") === "1"; } catch (e) { /* storage blocked */ }
    if (hint && wanted !== page && !dismissed) {
        hint.classList.add("show");
        hint.querySelector("button").addEventListener("click", function () {
            hint.classList.remove("show");
            try { localStorage.setItem("kamlynk-lang-hint", "1"); } catch (e) { /* storage blocked */ }
        });
    }
})();
