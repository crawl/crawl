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

    var pwaApi = window.DCSS_PWA || {};
    pwaApi.enabled = enabled;
    pwaApi.standalone = standalone;
    pwaApi.optionMinimums = optionMinimums;
    pwaApi.viewportMinDiameter = 15;
    window.DCSS_PWA = pwaApi;

    if (!enabled)
        return;

    document.documentElement.classList.add("dcss-pwa-mobile");
    if (standalone)
        document.documentElement.classList.add("dcss-pwa-standalone");

    var controls = null;
    var gameStateObserver = null;
    var controlsWatchTimer = null;
    var patchedJquery = false;
    var resizeQueued = false;
    var dispatchingResize = false;
    var dungeonTouch = null;

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
        catch
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
        if (active && dungeonTouch)
            dungeonTouch.bind();
        else if (!active && dungeonTouch)
            dungeonTouch.cleanup();
        if (controls)
        {
            controls.bindStatusObserver();
            controls.maybeAutoControlsMode();
        }
    }

    function startControlsWatch()
    {
        if (controlsWatchTimer)
            return;

        controlsWatchTimer = window.setInterval(function () {
            if (controls && controls.needsWatch())
                syncGameState();
        }, 250);
    }

    function controlsHeight()
    {
        return controls ? controls.height() : 0;
    }

    function controlsWidth()
    {
        return controls ? controls.width() : 0;
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
            if (controls)
                controls.updateHeight();
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

    function bindGameLifecycle($)
    {
        function enterGame()
        {
            setInGameState(true);
            if (dungeonTouch)
                dungeonTouch.bind();
        }

        function exitGame()
        {
            setInGameState(false);
            if (dungeonTouch)
                dungeonTouch.cleanup();
        }

        $(document).off(".dcss_pwa");
        $(document).on("game_init.dcss_pwa", enterGame);
        $(document).on("game_cleanup.dcss_pwa", exitGame);

        syncGameState();
        if (dungeonTouch)
            dungeonTouch.bind();

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
            if (window.DCSS_PWA.createDungeonTouch)
            {
                dungeonTouch = window.DCSS_PWA.createDungeonTouch({
                    $: $,
                    longPressDelay: 425,
                    tapMoveSlop: 14
                });
            }
            patchJqueryHeight($);
            bindGameLifecycle($);
            if (window.DCSS_PWA.createTouchControls)
            {
                controls = window.DCSS_PWA.createTouchControls({
                    sendInput: sendInput,
                    sendKey: sendKey,
                    requestRelayout: requestRelayout,
                    updateControlsHeight: updateControlsHeight,
                    startControlsWatch: startControlsWatch
                });
                controls.build(comm);
            }
        });
    });
})();
