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
    var optionMinimums = {
        tile_viewport_scale: 150
    };

    window.DCSS_PWA = {
        enabled: enabled,
        standalone: standalone,
        optionMinimums: optionMinimums,
        viewportMinDiameter: 11
    };

    if (!enabled)
        return;

    document.documentElement.classList.add("dcss-pwa-mobile");
    if (standalone)
        document.documentElement.classList.add("dcss-pwa-standalone");

    var controlsRoot = null;
    var gameStateObserver = null;
    var patchedJquery = false;
    var resizeQueued = false;
    var dispatchingResize = false;

    function setMobileViewport()
    {
        var viewport = document.querySelector("meta[name='viewport']");
        if (!viewport)
            return;
        viewport.setAttribute("content",
            "width=device-width, initial-scale=1, viewport-fit=cover");
    }

    function ready(fn)
    {
        if (document.readyState === "loading")
            document.addEventListener("DOMContentLoaded", fn);
        else
            fn();
    }

    function configureRequire()
    {
        if (!window.require || !window.require.config)
            return;

        window.require.config({
            baseUrl: "/static/scripts",
            paths: {
                "jquery": "/static/scripts/contrib/jquery"
            },
            shim: {
                "contrib/jquery.json": ["jquery"]
            },
            waitSeconds: 0,
            urlArgs: window.game_version
                ? "v=" + encodeURIComponent(window.game_version)
                : undefined
        });
    }

    function makeSessionId()
    {
        var bytes = new Uint32Array(2);
        if (window.crypto && window.crypto.getRandomValues)
        {
            window.crypto.getRandomValues(bytes);
            return bytes[0].toString(36) + bytes[1].toString(36);
        }
        return Date.now().toString(36)
            + Math.floor(Math.random() * 0xffffffff).toString(36);
    }

    function pwaSessionId()
    {
        try
        {
            var id = window.sessionStorage.getItem("dcss_pwa_session");
            if (!id)
            {
                id = makeSessionId();
                window.sessionStorage.setItem("dcss_pwa_session", id);
            }
            return id;
        }
        catch (error)
        {
            return makeSessionId();
        }
    }

    function tagSocketServer()
    {
        if (!window.socket_server || window.socket_server.indexOf("pwa_session=") !== -1)
            return;

        var separator = window.socket_server.indexOf("?") === -1 ? "?" : "&";
        window.socket_server += separator + "pwa_session="
            + encodeURIComponent(pwaSessionId());
    }

    function isInGame()
    {
        return document.documentElement.classList.contains("dcss-pwa-in-game");
    }

    function hasGameSurface()
    {
        return !!(document.getElementById("normal")
            && document.getElementById("dungeon"));
    }

    function setInGameState(active)
    {
        if (active === isInGame())
            return;
        document.documentElement.classList.toggle("dcss-pwa-in-game", active);
        requestRelayout();
    }

    function syncGameState()
    {
        setInGameState(hasGameSurface());
    }

    function controlsHeight()
    {
        return controlsRoot ? controlsRoot.offsetHeight : 0;
    }

    function visualHeight()
    {
        if (window.visualViewport)
            return Math.floor(window.visualViewport.height);
        return window.innerHeight || document.documentElement.clientHeight;
    }

    function safeTopInset()
    {
        var padding = window.getComputedStyle(document.body).paddingTop;
        var value = parseFloat(padding);
        return Number.isFinite(value) ? value : 0;
    }

    function usableGameHeight()
    {
        if (!isInGame())
            return null;

        var usable = visualHeight() - controlsHeight() - safeTopInset();
        return Math.max(240, Math.floor(usable));
    }

    function requestRelayout()
    {
        if (resizeQueued)
            return;

        resizeQueued = true;
        window.requestAnimationFrame(function () {
            resizeQueued = false;
            if (controlsRoot)
                updateControlsHeight(controlsRoot);
            dispatchingResize = true;
            window.dispatchEvent(new Event("resize"));
            dispatchingResize = false;
        });
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

    function movementKey(label, input, direction)
    {
        return {
            label: label + direction,
            input: input,
            className: "is-move",
            ariaLabel: label + " " + direction
        };
    }

    function inputKey(label, input, className)
    {
        return {
            label: label,
            input: input || label,
            className: className || ""
        };
    }

    function keycodeKey(label, keycode, className)
    {
        return {
            label: label,
            keycode: keycode,
            className: className || "is-action"
        };
    }

    function actionKey(label, action, className, span)
    {
        return {
            label: label,
            action: action,
            className: className || "is-system",
            span: span || 1
        };
    }

    var keyboardRows = [
        [
            inputKey("q"), inputKey("w"), inputKey("e"), inputKey("r"),
            inputKey("t"), movementKey("y", "y", "↖"),
            movementKey("u", "u", "↗"), inputKey("i"), inputKey("o"),
            inputKey("p")
        ],
        [
            inputKey("a"), inputKey("s"), inputKey("d"), inputKey("f"),
            inputKey("g"), movementKey("h", "h", "←"),
            movementKey("j", "j", "↓"), movementKey("k", "k", "↑"),
            movementKey("l", "l", "→"), keycodeKey("⌫", 8)
        ],
        [
            keycodeKey("⇥", 9), inputKey("z"), inputKey("x"), inputKey("c"),
            inputKey("v"), movementKey("b", "b", "↙"),
            movementKey("n", "n", "↘"), inputKey("m"), inputKey(";"),
            inputKey("'")
        ],
        [
            actionKey("⇧", "shift", "is-modifier", 2),
            actionKey("Ctrl", "ctrl", "is-modifier", 2),
            keycodeKey("Esc", 27, "is-action"),
            inputKey("5"), inputKey("-"), inputKey("+"),
            keycodeKey("↵", 13, "is-enter", 2),
            actionKey("123", "symbols", "is-modifier", 2)
        ]
    ];

    var symbolRows = [
        [
            inputKey("1"), inputKey("2"), inputKey("3"), inputKey("4"),
            inputKey("5"), inputKey("6"), inputKey("7"), inputKey("8"),
            inputKey("9"), inputKey("0")
        ],
        [
            inputKey("<"), inputKey(">"), inputKey("["), inputKey("]"),
            inputKey("{"), inputKey("}"), inputKey("/"), inputKey("*"),
            inputKey("="), keycodeKey("⌫", 8)
        ],
        [
            keycodeKey("⇥", 9), inputKey("!"), inputKey("?"), inputKey(":"),
            inputKey("\""), inputKey(","), inputKey("."), inputKey("_"),
            inputKey("@"), inputKey("#")
        ],
        [
            actionKey("⇧", "shift", "is-modifier", 2),
            actionKey("Ctrl", "ctrl", "is-modifier", 2),
            keycodeKey("Esc", 27, "is-action"),
            inputKey("`"), inputKey("-"), inputKey("+"),
            keycodeKey("↵", 13, "is-enter", 2),
            actionKey("ABC", "symbols", "is-modifier", 2)
        ]
    ];

    var shiftedInputs = {
        "1": "!",
        "2": "@",
        "3": "#",
        "4": "$",
        "5": "%",
        "6": "^",
        "7": "&",
        "8": "*",
        "9": "(",
        "0": ")",
        "-": "_",
        "=": "+",
        "`": "~",
        "[": "{",
        "]": "}",
        ";": ":",
        "'": "\"",
        ",": "<",
        ".": ">",
        "/": "?"
    };

    var keyboardState = {
        shift: false,
        ctrl: false,
        symbols: false
    };

    function applyShift(input)
    {
        if (!keyboardState.shift)
            return input;
        if (/^[a-z]$/.test(input))
            return input.toUpperCase();
        return shiftedInputs[input] || input;
    }

    function clearOneShotModifiers()
    {
        keyboardState.shift = false;
        keyboardState.ctrl = false;
    }

    function sendCtrlInput(comm, input)
    {
        var key = input.toUpperCase();
        if (!/^[A-Z0-9]$/.test(key))
            return false;

        sendKey(comm, key.charCodeAt(0) - "A".charCodeAt(0) + 1);
        return true;
    }

    function updateControlsHeight(root)
    {
        document.documentElement.style.setProperty(
            "--dcss-pwa-controls-height", root.offsetHeight + "px");
    }

    function patchJqueryHeight($)
    {
        if (patchedJquery)
            return;

        patchedJquery = true;
        var originalHeight = $.fn.height;
        $.fn.height = function () {
            if (arguments.length === 0 && this.length && this[0] === window)
            {
                var height = usableGameHeight();
                if (height !== null)
                    return height;
            }

            return originalHeight.apply(this, arguments);
        };
    }

    function keyboardRowsToRender()
    {
        return keyboardState.symbols ? symbolRows : keyboardRows;
    }

    function renderKeyboard(root)
    {
        root.textContent = "";
        root.classList.toggle("is-shift-active", keyboardState.shift);
        root.classList.toggle("is-ctrl-active", keyboardState.ctrl);
        root.classList.toggle("is-symbols-active", keyboardState.symbols);

        var keyboard = document.createElement("div");
        keyboard.className = "dcss-pwa-keyboard";

        keyboardRowsToRender().forEach(function (row) {
            var rowElement = document.createElement("div");
            var columns = 0;
            rowElement.className = "dcss-pwa-key-row";

            row.forEach(function (key) {
                columns += key.span || 1;
            });
            rowElement.style.setProperty("--dcss-pwa-key-columns", columns);

            row.forEach(function (key) {
                var button = makeButton(key.label, {
                    "class": key.className || "",
                    "aria-label": key.ariaLabel || key.label
                });
                button.style.setProperty(
                    "--dcss-pwa-key-span", key.span || 1);
                if (key.input)
                    button.dataset.input = key.input;
                if (key.keycode)
                    button.dataset.key = key.keycode;
                if (key.action)
                {
                    button.dataset.action = key.action;
                    button.setAttribute("aria-pressed",
                        keyboardState[key.action] ? "true" : "false");
                    if (keyboardState[key.action])
                        button.classList.add("is-active");
                }
                rowElement.appendChild(button);
            });

            keyboard.appendChild(rowElement);
        });

        root.appendChild(keyboard);
        updateControlsHeight(root);
        requestRelayout();
    }

    function buildControls(comm)
    {
        if (document.getElementById("dcss-pwa-controls"))
            return;

        var root = document.createElement("div");
        root.id = "dcss-pwa-controls";
        root.setAttribute("aria-label", "Crawl touch controls");
        document.body.appendChild(root);
        controlsRoot = root;
        renderKeyboard(root);

        root.addEventListener("click", function (event) {
            var button = event.target.closest("button");
            if (!button)
                return;

            event.preventDefault();
            if (button.dataset.action)
            {
                keyboardState[button.dataset.action] =
                    !keyboardState[button.dataset.action];
                if (button.dataset.action === "symbols")
                {
                    keyboardState.shift = false;
                    keyboardState.ctrl = false;
                }
                renderKeyboard(root);
                return;
            }

            if (button.dataset.key)
                sendKey(comm, parseInt(button.dataset.key, 10));
            else if (button.dataset.input)
            {
                if (keyboardState.ctrl)
                    sendCtrlInput(comm, button.dataset.input);
                else
                    sendInput(comm, applyShift(button.dataset.input));
            }

            clearOneShotModifiers();
            renderKeyboard(root);
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

    function bindGameLifecycle($)
    {
        function enterGame()
        {
            setInGameState(true);
        }

        function exitGame()
        {
            setInGameState(false);
        }

        $(document).off(".dcss_pwa");
        $(document).on("game_init.dcss_pwa", enterGame);
        $(document).on("game_cleanup.dcss_pwa", exitGame);

        syncGameState();

        if (gameStateObserver)
            return;

        gameStateObserver = new MutationObserver(syncGameState);
        gameStateObserver.observe(document.body, {
            childList: true,
            subtree: true
        });
        window.setTimeout(syncGameState, 0);
        window.setTimeout(syncGameState, 250);
        window.setTimeout(syncGameState, 1000);
    }

    function bindViewportChanges()
    {
        window.addEventListener("orientationchange", requestRelayout);
        window.addEventListener("resize", function () {
            if (!dispatchingResize)
                requestRelayout();
        });
        if (window.visualViewport)
            window.visualViewport.addEventListener("resize", requestRelayout);
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
        setMobileViewport();
        tagSocketServer();
        bindViewportChanges();
        registerServiceWorker();
        configureRequire();
        window.require(["jquery", "comm"], function ($, comm) {
            patchJqueryHeight($);
            bindGameLifecycle($);
            buildControls(comm);
        });
    });
})();
