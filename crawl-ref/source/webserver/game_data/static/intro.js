// Keep the intro in the per-game client: public servers share client.js
// between Crawl versions and do not replace it when installing a fork.
define(["require", "jquery", "comm", "client"],
function (require, $, comm, client) {
    "use strict";

    var overlay = null;
    var timer = null;

    function block_input(event)
    {
        // Prevent typing through to character creation behind the splash,
        // while retaining browser shortcuts such as reload and close tab.
        if (!event.ctrlKey && !event.altKey && !event.metaKey)
            event.preventDefault();
        event.stopImmediatePropagation();
    }

    function cleanup()
    {
        window.clearTimeout(timer);
        timer = null;
        document.removeEventListener("keydown", block_input, true);
        document.removeEventListener("keypress", block_input, true);
        document.removeEventListener("keyup", block_input, true);
        if (overlay)
        {
            overlay.find("img").off(".dcchili_intro");
            overlay.remove();
            overlay = null;
        }
    }

    function show()
    {
        cleanup();
        if (client.is_watching())
            return;

        overlay = $("<div>", { id: "dcchili_intro" }).css({
            position: "fixed",
            top: 0,
            right: 0,
            bottom: 0,
            left: 0,
            zIndex: 10000,
            background: "black",
            display: "flex",
            alignItems: "center",
            justifyContent: "center"
        }).appendTo("body");

        var image = $("<img>", { alt: "Dungeon Crawl Chili" }).css({
            maxWidth: "90vw",
            maxHeight: "90vh"
        }).appendTo(overlay);
        document.addEventListener("keydown", block_input, true);
        document.addEventListener("keypress", block_input, true);
        document.addEventListener("keyup", block_input, true);

        // A missing or stalled image must not prevent starting a game.
        timer = window.setTimeout(cleanup, 10000);
        function loaded()
        {
            if (!overlay)
                return;
            window.clearTimeout(timer);
            timer = window.setTimeout(cleanup, 3000);
        }
        image.one("load.dcchili_intro", loaded);
        image.one("error.dcchili_intro", cleanup);
        image.attr("src", require.toUrl("./dcchili_intro.png"));
        if (image[0].complete)
        {
            if (image[0].naturalWidth)
                loaded();
            else
                cleanup();
        }
    }

    $(document).on("game_preinit.dcchili_intro game_cleanup.dcchili_intro", cleanup);
    comm.register_handlers({ "dcchili_intro": show });
});
