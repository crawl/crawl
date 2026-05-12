(function () {
    "use strict";

    var search = window.location.search || "";
    var forcedOn = /(?:\?|&)pwa=1(?:&|$)/.test(search);
    var forcedOff = /(?:\?|&)pwa=0(?:&|$)/.test(search);
    var standalone = window.navigator.standalone
        || (window.matchMedia
            && window.matchMedia("(display-mode: standalone)").matches);
    var iPhoneTouch = /iPhone|iPod/.test(window.navigator.userAgent)
        && "ontouchstart" in window;
    var enabled = !forcedOff && (forcedOn || standalone || iPhoneTouch);

    window.DCSS_PWA = {
        enabled: enabled,
        standalone: standalone
    };

    if (!enabled)
        return;

    document.documentElement.classList.add("dcss-pwa-mobile");
    if (standalone)
        document.documentElement.classList.add("dcss-pwa-standalone");

    var viewport = document.querySelector("meta[name='viewport']");
    if (viewport)
        viewport.setAttribute("content",
            "width=device-width, initial-scale=1, viewport-fit=cover, user-scalable=no");

    function ready(fn)
    {
        if (document.readyState === "loading")
            document.addEventListener("DOMContentLoaded", fn);
        else
            fn();
    }

    function sendInput(comm, text)
    {
        if (!window.socket || window.socket.readyState !== WebSocket.OPEN)
            return;
        comm.send_message("text_input", { text: text });
    }

    function sendKey(comm, keycode)
    {
        if (!window.socket || window.socket.readyState !== WebSocket.OPEN)
            return;
        comm.send_message("key", { keycode: keycode });
    }

    function makeButton(label, attrs)
    {
        var button = document.createElement("button");
        button.type = "button";
        button.textContent = label;
        Object.keys(attrs || {}).forEach(function (name) {
            button.setAttribute(name, attrs[name]);
        });
        return button;
    }

    function updateControlsHeight(root)
    {
        document.documentElement.style.setProperty(
            "--dcss-pwa-controls-height", root.offsetHeight + "px");
    }

    function buildControls(comm)
    {
        if (document.getElementById("dcss-pwa-controls"))
            return;

        var root = document.createElement("div");
        root.id = "dcss-pwa-controls";
        root.setAttribute("aria-label", "Crawl touch controls");

        var movePad = document.createElement("div");
        movePad.className = "dcss-pwa-move-pad";
        [
            ["NW", "y"], ["N", "k"], ["NE", "u"],
            ["W", "h"], ["Wait", "."], ["E", "l"],
            ["SW", "b"], ["S", "j"], ["SE", "n"]
        ].forEach(function (item) {
            movePad.appendChild(makeButton(item[0], {
                "data-input": item[1],
                "aria-label": item[0]
            }));
        });

        var commandRow = document.createElement("div");
        commandRow.className = "dcss-pwa-command-row";
        [
            ["Move", { "data-toggle": "move" }],
            ["Auto", { "data-input": "o" }],
            ["Wait", { "data-input": "." }],
            ["Inv", { "data-input": "i" }],
            ["Abil", { "data-input": "a" }],
            ["Spell", { "data-input": "z" }],
            ["Fire", { "data-input": "f" }],
            ["Esc", { "data-key": "27" }],
            ["Enter", { "data-key": "13" }]
        ].forEach(function (item) {
            commandRow.appendChild(makeButton(item[0], item[1]));
        });

        root.appendChild(movePad);
        root.appendChild(commandRow);
        document.body.appendChild(root);

        root.addEventListener("click", function (event) {
            var button = event.target.closest("button");
            if (!button)
                return;

            event.preventDefault();
            if (button.dataset.toggle === "move")
            {
                root.classList.toggle("is-open");
                updateControlsHeight(root);
                window.dispatchEvent(new Event("resize"));
                return;
            }
            if (button.dataset.key)
                sendKey(comm, parseInt(button.dataset.key, 10));
            else if (button.dataset.input)
                sendInput(comm, button.dataset.input);
        });

        if ("ResizeObserver" in window)
        {
            var observer = new ResizeObserver(function () {
                updateControlsHeight(root);
            });
            observer.observe(root);
        }
        updateControlsHeight(root);
    }

    function registerServiceWorker()
    {
        if (!("serviceWorker" in navigator))
            return;
        navigator.serviceWorker.register("/service-worker.js", { scope: "/" })
            .catch(function (error) {
                console.warn("PWA service worker registration failed:", error);
            });
    }

    ready(function () {
        registerServiceWorker();
        window.require(["comm"], function (comm) {
            buildControls(comm);
        });
    });
})();
