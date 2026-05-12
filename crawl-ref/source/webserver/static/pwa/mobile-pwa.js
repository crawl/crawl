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
    var jqueryApi = null;
    var patchedJquery = false;
    var resizeQueued = false;
    var dispatchingResize = false;
    var controlsMode = "keyboard";
    var userSelectedControlsMode = false;
    var statusRoot = null;
    var statusObserver = null;
    var renderingControls = false;
    var activePointerButton = null;
    var activePointerId = null;
    var repeatDelayTimer = null;
    var repeatIntervalTimer = null;
    var suppressClickUntil = 0;
    var holdRepeatDelay = 300;
    var holdRepeatInterval = 85;
    var dungeonTouchElement = null;
    var dungeonTouchCleanup = null;
    var dungeonPress = null;
    var dungeonLongPressTimer = null;
    var dungeonLongPressDelay = 425;
    var dungeonTapMoveSlop = 14;

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
        var active = hasGameSurface();
        setInGameState(active);
        if (active && jqueryApi)
            bindDungeonTouch(jqueryApi);
        else if (!active && dungeonTouchCleanup)
            dungeonTouchCleanup();
        bindStatusObserver();
        maybeAutoControlsMode();
    }

    function controlsHeight()
    {
        return controlsRoot ? controlsRoot.offsetHeight : 0;
    }

    function controlsWidth()
    {
        return controlsRoot ? controlsRoot.offsetWidth : 0;
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

    function sideControlsActive()
    {
        return isInGame() && window.matchMedia
            && window.matchMedia("(orientation: landscape) and (max-height: 520px)").matches;
    }

    function usableGameHeight()
    {
        if (!isInGame())
            return null;

        if (sideControlsActive())
            return Math.max(240, Math.floor(visualHeight() - safeTopInset()));

        var usable = visualHeight() - controlsHeight() - safeTopInset();
        return Math.max(240, Math.floor(usable));
    }

    function usableGameWidth()
    {
        if (!sideControlsActive())
            return null;

        return Math.max(320, Math.floor(window.innerWidth - controlsWidth()));
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
            inputKey("5"), actionKey("Pad", "compact", "is-system", 2),
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
            inputKey("-"), inputKey("+"),
            actionKey("Pad", "compact", "is-system", 2),
            keycodeKey("↵", 13, "is-enter", 2),
            actionKey("ABC", "symbols", "is-modifier", 2)
        ]
    ];

    var compactRows = [
        [
            movementKey("y", "y", "↖"), movementKey("k", "k", "↑"),
            movementKey("u", "u", "↗"), keycodeKey("Tab", 9, "is-action"),
            keycodeKey("Esc", 27, "is-action"), keycodeKey("↵", 13, "is-enter")
        ],
        [
            movementKey("h", "h", "←"), inputKey("5", "5", "is-move"),
            movementKey("l", "l", "→"), inputKey("o"), inputKey("i"),
            inputKey("g")
        ],
        [
            movementKey("b", "b", "↙"), movementKey("j", "j", "↓"),
            movementKey("n", "n", "↘"), inputKey("<"), inputKey(">"),
            actionKey("ABC", "keyboard", "is-modifier")
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
        var changed = keyboardState.shift || keyboardState.ctrl;
        keyboardState.shift = false;
        keyboardState.ctrl = false;
        return changed;
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
        var sideControls = sideControlsActive();
        document.documentElement.classList.toggle(
            "dcss-pwa-side-controls", sideControls);
        document.documentElement.style.setProperty(
            "--dcss-pwa-controls-height",
            (sideControls ? 0 : root.offsetHeight) + "px");
        document.documentElement.style.setProperty(
            "--dcss-pwa-controls-width",
            (sideControls ? root.offsetWidth : 0) + "px");
    }

    function statText(id)
    {
        var element = document.getElementById(id);
        return element ? element.textContent.trim() : "";
    }

    function compactPlace()
    {
        return statText("stats_place").replace(/^Dungeon:/, "D:");
    }

    function hasLiveStats()
    {
        var maxHp = parseInt(statText("stats_hp_max"), 10);
        return Number.isFinite(maxHp) && maxHp > 0;
    }

    function hasVisiblePopup()
    {
        var popups = document.querySelectorAll("#ui-stack > .ui-popup");
        return Array.prototype.some.call(popups, function (popup) {
            var rect = popup.getBoundingClientRect();
            return rect.width > 0 && rect.height > 0
                && !popup.classList.contains("hidden");
        });
    }

    function preferredControlsMode()
    {
        if (hasVisiblePopup())
            return "keyboard";
        if (hasLiveStats())
            return "compact";
        return "keyboard";
    }

    function maybeAutoControlsMode()
    {
        if (!controlsRoot)
        {
            return;
        }

        if (!hasVisiblePopup() && userSelectedControlsMode)
            return;

        var mode = preferredControlsMode();
        if (mode === controlsMode)
            return;

        controlsMode = mode;
        if (!renderingControls)
            renderControls(controlsRoot);
    }

    function syncStatus()
    {
        if (!statusRoot)
            return;

        var items = [
            "HP " + statText("stats_hp") + "/" + statText("stats_hp_max"),
            "MP " + statText("stats_mp") + "/" + statText("stats_mp_max"),
            "XL" + statText("stats_xl"),
            "AC" + statText("stats_ac"),
            "EV" + statText("stats_ev"),
            "SH" + statText("stats_sh"),
            compactPlace()
        ].filter(function (item) {
            return !/\/$/.test(item) && !/[A-Z]$/.test(item);
        });

        statusRoot.textContent = "";
        items.forEach(function (item) {
            var span = document.createElement("span");
            span.textContent = item;
            statusRoot.appendChild(span);
        });

        maybeAutoControlsMode();
    }

    function renderStatus(root)
    {
        statusRoot = document.createElement("div");
        statusRoot.className = "dcss-pwa-status";
        root.appendChild(statusRoot);
        syncStatus();
    }

    function bindStatusObserver()
    {
        var stats = document.getElementById("stats");
        if (!stats || statusObserver)
            return;

        statusObserver = new MutationObserver(syncStatus);
        statusObserver.observe(stats, {
            childList: true,
            characterData: true,
            subtree: true,
            attributes: true
        });
        syncStatus();
    }

    function resetDungeonTouchState()
    {
        if (dungeonLongPressTimer)
        {
            window.clearTimeout(dungeonLongPressTimer);
            dungeonLongPressTimer = null;
        }
        dungeonPress = null;
    }

    function touchPoint(touch)
    {
        return {
            clientX: touch.clientX,
            clientY: touch.clientY,
            pageX: touch.pageX,
            pageY: touch.pageY
        };
    }

    function movedBeyondDungeonTapSlop(point)
    {
        if (!dungeonPress || !point)
            return true;

        var dx = point.clientX - dungeonPress.start.clientX;
        var dy = point.clientY - dungeonPress.start.clientY;
        return dx * dx + dy * dy > dungeonTapMoveSlop * dungeonTapMoveSlop;
    }

    function updateDungeonPress(point)
    {
        if (!dungeonPress || !point)
            return;

        dungeonPress.latest = point;
        if (movedBeyondDungeonTapSlop(point))
            dungeonPress.moved = true;
    }

    function clearDungeonLongPressTimer()
    {
        if (!dungeonLongPressTimer)
            return;

        window.clearTimeout(dungeonLongPressTimer);
        dungeonLongPressTimer = null;
    }

    function triggerDungeonMouse($, element, point, which)
    {
        if (!point)
            return;

        $(element).trigger($.Event("mousedown", {
            which: which,
            button: which === 3 ? 2 : 0,
            buttons: which === 3 ? 2 : 1,
            clientX: point.clientX,
            clientY: point.clientY,
            pageX: point.pageX,
            pageY: point.pageY
        }));
    }

    function startDungeonPress($, element, point)
    {
        resetDungeonTouchState();
        dungeonPress = {
            start: point,
            latest: point,
            moved: false,
            longPressed: false
        };

        dungeonLongPressTimer = window.setTimeout(function () {
            dungeonLongPressTimer = null;
            if (!dungeonPress || dungeonPress.moved)
                return;

            dungeonPress.longPressed = true;
            triggerDungeonMouse($, element, dungeonPress.latest, 3);
        }, dungeonLongPressDelay);
    }

    function finishDungeonPress($, element)
    {
        if (!dungeonPress)
            return;

        clearDungeonLongPressTimer();
        if (!dungeonPress.longPressed && !dungeonPress.moved)
            triggerDungeonMouse($, element, dungeonPress.latest, 1);

        resetDungeonTouchState();
    }

    function bindDungeonTouch($)
    {
        var dungeon = document.getElementById("dungeon");
        if (!dungeon)
            return;

        if (dungeonTouchElement === dungeon)
            return;

        if (dungeonTouchElement)
        {
            if (dungeonTouchCleanup)
                dungeonTouchCleanup();
            else
                dungeonTouchElement = null;
        }

        dungeonTouchElement = dungeon;

        function cancelEvent(event)
        {
            event.preventDefault();
        }

        function onTouchStart(event)
        {
            cancelEvent(event);
            if (event.touches.length !== 1)
            {
                resetDungeonTouchState();
                return;
            }

            startDungeonPress($, dungeon, touchPoint(event.touches[0]));
        }

        function onTouchMove(event)
        {
            if (!dungeonPress)
                return;

            cancelEvent(event);
            if (event.touches.length !== 1)
            {
                resetDungeonTouchState();
                return;
            }

            updateDungeonPress(touchPoint(event.touches[0]));
            if (dungeonPress && dungeonPress.moved)
                clearDungeonLongPressTimer();
        }

        function onTouchEnd(event)
        {
            if (!dungeonPress)
                return;

            cancelEvent(event);
            if (event.changedTouches.length)
                updateDungeonPress(touchPoint(event.changedTouches[0]));

            if (event.touches.length === 0)
                finishDungeonPress($, dungeon);
            else
                resetDungeonTouchState();
        }

        function onTouchCancel(event)
        {
            cancelEvent(event);
            resetDungeonTouchState();
        }

        function onContextMenu(event)
        {
            cancelEvent(event);
        }

        dungeon.addEventListener("touchstart", onTouchStart, { passive: false });
        dungeon.addEventListener("touchmove", onTouchMove, { passive: false });
        dungeon.addEventListener("touchend", onTouchEnd, { passive: false });
        dungeon.addEventListener("touchcancel", onTouchCancel,
            { passive: false });
        dungeon.addEventListener("contextmenu", onContextMenu);

        dungeonTouchCleanup = function () {
            dungeon.removeEventListener("touchstart", onTouchStart);
            dungeon.removeEventListener("touchmove", onTouchMove);
            dungeon.removeEventListener("touchend", onTouchEnd);
            dungeon.removeEventListener("touchcancel", onTouchCancel);
            dungeon.removeEventListener("contextmenu", onContextMenu);
            resetDungeonTouchState();
            dungeonTouchCleanup = null;
            dungeonTouchElement = null;
        };
    }

    function patchJqueryHeight($)
    {
        if (patchedJquery)
            return;

        patchedJquery = true;
        var originalHeight = $.fn.height;
        var originalWidth = $.fn.width;
        $.fn.height = function () {
            if (arguments.length === 0 && this.length && this[0] === window)
            {
                var height = usableGameHeight();
                if (height !== null)
                    return height;
            }

            return originalHeight.apply(this, arguments);
        };
        $.fn.width = function () {
            if (arguments.length === 0 && this.length && this[0] === window)
            {
                var width = usableGameWidth();
                if (width !== null)
                    return width;
            }

            return originalWidth.apply(this, arguments);
        };
    }

    function keyboardRowsToRender()
    {
        return keyboardState.symbols ? symbolRows : keyboardRows;
    }

    function makeControlButton(key)
    {
        var button = makeButton(key.label, {
            "class": key.className || "",
            "aria-label": key.ariaLabel || key.label
        });

        button.style.setProperty("--dcss-pwa-key-span", key.span || 1);
        if (key.input)
            button.dataset.input = key.input;
        if (key.keycode)
            button.dataset.key = key.keycode;
        if (key.action)
        {
            button.dataset.action = key.action;
            button.setAttribute("aria-pressed",
                keyboardState[key.action] ? "true" : "false");
            if (keyboardState[key.action]
                || (key.action === "compact" && controlsMode === "compact")
                || (key.action === "keyboard" && controlsMode === "keyboard"))
            {
                button.classList.add("is-active");
            }
        }

        return button;
    }

    function renderFullKeyboard(root)
    {
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
                rowElement.appendChild(makeControlButton(key));
            });

            keyboard.appendChild(rowElement);
        });

        root.appendChild(keyboard);
    }

    function renderCompactControls(root)
    {
        var compact = document.createElement("div");
        compact.className = "dcss-pwa-compact";

        compactRows.forEach(function (row) {
            row.forEach(function (key) {
                compact.appendChild(makeControlButton(key));
            });
        });

        root.appendChild(compact);
    }

    function renderControls(root)
    {
        renderingControls = true;
        root.textContent = "";
        root.classList.toggle("is-compact-mode", controlsMode === "compact");
        root.classList.toggle("is-keyboard-mode", controlsMode === "keyboard");
        root.classList.toggle("is-shift-active", keyboardState.shift);
        root.classList.toggle("is-ctrl-active", keyboardState.ctrl);
        root.classList.toggle("is-symbols-active", keyboardState.symbols);

        renderStatus(root);
        if (controlsMode === "compact")
            renderCompactControls(root);
        else
            renderFullKeyboard(root);

        updateControlsHeight(root);
        renderingControls = false;
        requestRelayout();
    }

    function handleControlAction(button, root)
    {
        var action = button.dataset.action;
        if (action === "compact" || action === "keyboard")
        {
            controlsMode = action;
            userSelectedControlsMode = true;
            keyboardState.shift = false;
            keyboardState.ctrl = false;
            keyboardState.symbols = false;
            renderControls(root);
            return true;
        }

        keyboardState[action] = !keyboardState[action];
        if (action === "symbols")
        {
            keyboardState.shift = false;
            keyboardState.ctrl = false;
        }
        renderControls(root);
        return true;
    }

    function sendButtonInput(button, comm)
    {
        if (button.dataset.key)
            sendKey(comm, parseInt(button.dataset.key, 10));
        else if (button.dataset.input)
        {
            if (keyboardState.ctrl)
                sendCtrlInput(comm, button.dataset.input);
            else
                sendInput(comm, applyShift(button.dataset.input));
        }
    }

    function isRepeatableButton(button)
    {
        return !button.dataset.action && button.classList.contains("is-move");
    }

    function clearHoldRepeat()
    {
        if (repeatDelayTimer)
        {
            window.clearTimeout(repeatDelayTimer);
            repeatDelayTimer = null;
        }
        if (repeatIntervalTimer)
        {
            window.clearInterval(repeatIntervalTimer);
            repeatIntervalTimer = null;
        }
    }

    function startHoldRepeat(button, comm)
    {
        clearHoldRepeat();
        if (!isRepeatableButton(button) || !controlsRoot.contains(button))
            return;

        repeatDelayTimer = window.setTimeout(function () {
            repeatDelayTimer = null;
            if (controlsRoot.contains(button))
                sendButtonInput(button, comm);
            else
                return;
            repeatIntervalTimer = window.setInterval(function () {
                if (controlsRoot.contains(button))
                    sendButtonInput(button, comm);
                else
                    clearHoldRepeat();
            }, holdRepeatInterval);
        }, holdRepeatDelay);
    }

    function releaseActivePointer(event)
    {
        clearHoldRepeat();
        if (activePointerButton)
        {
            activePointerButton.classList.remove("is-pressed");
            if (activePointerButton.releasePointerCapture
                && activePointerId !== null)
            {
                try
                {
                    activePointerButton.releasePointerCapture(activePointerId);
                }
                catch (error)
                {
                }
            }
        }
        activePointerButton = null;
        activePointerId = null;
        if (event)
            event.preventDefault();
    }

    function invokeControlButton(button, root, comm)
    {
        if (button.dataset.action)
        {
            handleControlAction(button, root);
            return false;
        }

        sendButtonInput(button, comm);

        if (clearOneShotModifiers())
            renderControls(root);

        return true;
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
        renderControls(root);

        root.addEventListener("pointerdown", function (event) {
            if (event.button !== undefined && event.button !== 0)
                return;

            var button = event.target.closest("button");
            if (!button || !root.contains(button))
                return;

            event.preventDefault();
            suppressClickUntil = Date.now() + 700;
            releaseActivePointer();
            activePointerButton = button;
            activePointerId = event.pointerId;
            button.classList.add("is-pressed");
            if (button.setPointerCapture && event.pointerId !== undefined)
            {
                try
                {
                    button.setPointerCapture(event.pointerId);
                }
                catch (error)
                {
                }
            }

            if (invokeControlButton(button, root, comm))
                startHoldRepeat(button, comm);
        });

        root.addEventListener("pointerup", function (event) {
            if (activePointerId !== null && event.pointerId !== activePointerId)
                return;
            releaseActivePointer(event);
        });

        root.addEventListener("pointercancel", function (event) {
            if (activePointerId !== null && event.pointerId !== activePointerId)
                return;
            releaseActivePointer(event);
        });

        root.addEventListener("pointerleave", function (event) {
            if (activePointerId !== null && event.pointerId !== activePointerId)
                return;
            releaseActivePointer(event);
        });

        root.addEventListener("click", function (event) {
            var button = event.target.closest("button");
            if (!button)
                return;

            event.preventDefault();
            if (Date.now() < suppressClickUntil)
                return;

            invokeControlButton(button, root, comm);
        });

        root.addEventListener("contextmenu", function (event) {
            if (event.target.closest("button"))
                event.preventDefault();
        });

        if ("ResizeObserver" in window)
        {
            var observer = new ResizeObserver(function () {
                updateControlsHeight(root);
            });
            observer.observe(root);
        }
        bindStatusObserver();
        updateControlsHeight(root);
    }

    function bindGameLifecycle($)
    {
        function enterGame()
        {
            setInGameState(true);
            bindDungeonTouch($);
        }

        function exitGame()
        {
            setInGameState(false);
            if (dungeonTouchCleanup)
                dungeonTouchCleanup();
            else
                resetDungeonTouchState();
        }

        $(document).off(".dcss_pwa");
        $(document).on("game_init.dcss_pwa", enterGame);
        $(document).on("game_cleanup.dcss_pwa", exitGame);

        syncGameState();
        bindDungeonTouch($);

        if (gameStateObserver)
            return;

        gameStateObserver = new MutationObserver(syncGameState);
        gameStateObserver.observe(document.body, {
            attributes: true,
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
            jqueryApi = $;
            patchJqueryHeight($);
            bindGameLifecycle($);
            buildControls(comm);
        });
    });
})();
