"use strict";

import { createControlKeymaps } from "./mobile-pwa-control-keymaps.js";

function createStatusStrip(options) {
    var statusRoot = null;
    var statusObserver = null;
    var observedStats = null;
    var onChange = options.onChange || function () {};

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

    function sync()
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

        onChange();
    }

    function render(root)
    {
        statusRoot = document.createElement("div");
        statusRoot.className = "dcss-pwa-status";
        root.appendChild(statusRoot);
        sync();
    }

    function bindObserver()
    {
        var stats = document.getElementById("stats");
        if (!stats)
            return;

        if (stats === observedStats && statusObserver)
            return;

        if (statusObserver)
            statusObserver.disconnect();

        observedStats = stats;
        statusObserver = new MutationObserver(sync);
        statusObserver.observe(stats, {
            childList: true,
            characterData: true,
            subtree: true,
            attributes: true
        });
        sync();
    }

    return {
        bindObserver: bindObserver,
        hasLiveStats: hasLiveStats,
        render: render,
        sync: sync
    };
}

export function createTouchControls(options) {
    var sendInput = options.sendInput;
    var sendKey = options.sendKey;
    var requestRelayout = options.requestRelayout;
    var updateControlsHeight = options.updateControlsHeight;
    var startControlsWatch = options.startControlsWatch;
    var controlsRoot = null;
    var controlsMode = "keyboard";
    var controlsHidden = false;
    var userSelectedControlsMode = false;
    var renderingControls = false;
    var activePointerButton = null;
    var activePointerId = null;
    var repeatDelayTimer = null;
    var repeatIntervalTimer = null;
    var suppressClickUntil = 0;
    var holdRepeatDelay = 300;
    var holdRepeatInterval = 85;
    var keymaps = createControlKeymaps();
    var statusStrip = createStatusStrip({
        onChange: function () {
            if (!renderingControls)
                maybeAutoControlsMode();
        }
    });

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

    function closestButton(target)
    {
        return target instanceof Element ? target.closest("button") : null;
    }

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
        if (statusStrip.hasLiveStats())
            return "compact";
        return "keyboard";
    }

    function maybeAutoControlsMode()
    {
        var controlsWereHidden = false;

        if (!controlsRoot)
            return;

        if (controlsHidden)
        {
            if (!hasVisiblePopup() && statusStrip.hasLiveStats())
                return;
            controlsHidden = false;
            userSelectedControlsMode = false;
            controlsWereHidden = true;
        }

        if (!hasVisiblePopup() && userSelectedControlsMode)
            return;

        var mode = preferredControlsMode();
        if (mode === controlsMode && !controlsWereHidden)
            return;

        controlsMode = mode;
        if (!renderingControls)
            renderControls(controlsRoot);
    }

    function keyboardRowsToRender()
    {
        return keyboardState.symbols
            ? keymaps.symbolRows : keymaps.keyboardRows;
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
                || (key.action === "compact"
                    && controlsMode === "compact")
                || (key.action === "keyboard"
                    && controlsMode === "keyboard"))
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
            rowElement.style.setProperty(
                "--dcss-pwa-key-columns", String(columns));

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

        keymaps.compactRows.forEach(function (row) {
            row.forEach(function (key) {
                compact.appendChild(makeControlButton(key));
            });
        });

        root.appendChild(compact);
    }

    function renderHideControl(root)
    {
        var button = makeControlButton(
            keymaps.actionKey("Hide", "minimize",
                "dcss-pwa-hide-control is-system"));
        root.appendChild(button);
    }

    function renderControls(root)
    {
        renderingControls = true;
        root.textContent = "";
        root.classList.toggle("is-minimized", controlsHidden);
        root.classList.toggle("is-compact-mode",
            controlsMode === "compact");
        root.classList.toggle("is-keyboard-mode",
            controlsMode === "keyboard");
        root.classList.toggle("is-shift-active", keyboardState.shift);
        root.classList.toggle("is-ctrl-active", keyboardState.ctrl);
        root.classList.toggle("is-symbols-active", keyboardState.symbols);

        if (controlsHidden)
        {
            root.appendChild(makeControlButton(
                keymaps.actionKey("⌨", "restore", "is-system")));
            updateControlsHeight(root);
            renderingControls = false;
            requestRelayout();
            return;
        }

        statusStrip.render(root);
        if (controlsMode === "compact")
            renderCompactControls(root);
        else
            renderFullKeyboard(root);
        renderHideControl(root);

        updateControlsHeight(root);
        renderingControls = false;
        requestRelayout();
    }

    function handleControlAction(button, root)
    {
        var action = button.dataset.action;
        if (action === "minimize" || action === "restore")
        {
            controlsHidden = action === "minimize";
            if (!controlsHidden)
                controlsMode = preferredControlsMode();
            userSelectedControlsMode = action === "minimize";
            keyboardState.shift = false;
            keyboardState.ctrl = false;
            keyboardState.symbols = false;
            renderControls(root);
            return true;
        }

        if (action === "compact" || action === "keyboard")
        {
            controlsHidden = false;
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
                catch
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

    function build(comm)
    {
        var existing = document.getElementById("dcss-pwa-controls");
        if (existing)
        {
            controlsRoot = existing;
            return;
        }

        var root = document.createElement("div");
        root.id = "dcss-pwa-controls";
        root.setAttribute("aria-label", "Crawl touch controls");
        document.body.appendChild(root);
        controlsRoot = root;
        renderControls(root);

        root.addEventListener("pointerdown", function (event) {
            if (event.button !== undefined && event.button !== 0)
                return;

            var button = closestButton(event.target);
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
                catch
                {
                }
            }

            if (invokeControlButton(button, root, comm))
                startHoldRepeat(button, comm);
        });

        root.addEventListener("pointerup", function (event) {
            if (activePointerId !== null
                && event.pointerId !== activePointerId)
            {
                return;
            }
            releaseActivePointer(event);
        });

        root.addEventListener("pointercancel", function (event) {
            if (activePointerId !== null
                && event.pointerId !== activePointerId)
            {
                return;
            }
            releaseActivePointer(event);
        });

        root.addEventListener("click", function (event) {
            var button = closestButton(event.target);
            if (!button)
                return;

            event.preventDefault();
            if (Date.now() < suppressClickUntil)
                return;

            invokeControlButton(button, root, comm);
        });

        root.addEventListener("contextmenu", function (event) {
            if (closestButton(event.target))
                event.preventDefault();
        });

        if ("ResizeObserver" in window)
        {
            var observer = new ResizeObserver(function () {
                updateControlsHeight(root);
            });
            observer.observe(root);
        }
        startControlsWatch();
        statusStrip.bindObserver();
        updateControlsHeight(root);
    }

    return {
        build: build,
        bindStatusObserver: statusStrip.bindObserver,
        maybeAutoControlsMode: maybeAutoControlsMode,
        syncStatus: statusStrip.sync,
        needsWatch: function () {
            return controlsHidden || hasVisiblePopup();
        },
        height: function () {
            return controlsRoot ? controlsRoot.offsetHeight : 0;
        },
        width: function () {
            return controlsRoot ? controlsRoot.offsetWidth : 0;
        },
        updateHeight: function () {
            if (controlsRoot)
                updateControlsHeight(controlsRoot);
        }
    };
}
