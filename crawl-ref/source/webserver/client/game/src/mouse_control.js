define(["jquery", "comm", "./dungeon_renderer", "./tileinfo-gui", "./map_knowledge",
        "./enums"],
function ($, comm, dr, gui, map_knowledge, enums) {

    function handle_cell_click(ev)
    {
        comm.send_message("click_cell", {
            x: ev.cell.x,
            y: ev.cell.y,
            button: ev.which
        });
    }

    function show_tooltip(text, x, y)
    {
        $("#tooltip")
            .html(text)
            .css({ left: x, top: y })
            .show();
    }

    function hide_tooltip()
    {
        $("#tooltip").hide();
    }

    function handle_cell_tooltip(ev)
    {
        var map_cell = map_knowledge.get(ev.cell.x, ev.cell.y);

        var text = ""
        // don't show a tooltip unless there's a monster in the square. It might
        // be good to expand this (local tiles does), but there are a number
        // of issues. First, I found the tooltips pretty disruptive in my
        // testing, most of the time you don't care. Second, there are some
        // challenges in filling tooltip info outside of a narrow slice of
        // cases where the info is neatly packaged for webtiles. E.g. a tooltip
        // that tells you whether you can move to a square is very hard to
        // construct on the client side (see tilerg-dgn.cc:tile_dungeon_tip for
        // the logic that would be necessary.)
        // minimal extensions: tooltips for interesting features, like statues,
        // doors, stairs, altars.

        if (!map_cell.mon)
            return;

        // `game` force-set in game.js because of circular reference issues
        if (game.can_target() && map_knowledge.visible(map_cell))
        {
            // XX just looking case has weird behavior
            text += "Left click: select target";
        }
        // XX a good left click tooltip for click travel is very hard to
        // construct on the client side...
        // see tilerg-dgn.cc:tile_dungeon_tip for the logic that would be
        // necessary.

        if (game.can_describe())
        {
            // only show right-click desc if there's something else in the
            // tooltip, it's too disruptive otherwise
            if (text)
                text += "<br>"; // XX something better than <br>s
            text += "Right click: describe";
        }

        if (text)
            text = "<br><br>" + text;
        text = map_cell.mon.name + text;
        show_tooltip(text, ev.pageX + 10, ev.pageY + 10);
    }

    $(document)
        .off("game_init.mouse_control")
        .on("game_init.mouse_control", function () {
            $("#dungeon")
                .on("cell_click", handle_cell_click)
                .on("cell_tooltip", handle_cell_tooltip)
                .contextmenu(function() { return false; });
        });
    $(window)
        .off("mouseup.mouse_control mousemove.mouse_control mousedown.mouse_control")
        .on("mousemove.mouse_control mousedown.mouse_control", function (ev) {
            hide_tooltip();
        });

    return {
        show_tooltip: show_tooltip,
        hide_tooltip: hide_tooltip
    };
});
