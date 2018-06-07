define(["jquery", "comm", "./dungeon_renderer", "./tileinfo-gui", "./map_knowledge",
        "./enums"],
function ($, comm, dr, gui, map_knowledge, enums) {
    function handle_cell_click(ev)
    {
        if (ev.which == 1) // Left click
        {
            comm.send_message("click_travel", {
                x: ev.cell.x,
                y: ev.cell.y
            });
        }
        else if (ev.which == 3) // Right click
        {
        }
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

        if (map_cell.mon)
            show_tooltip(map_cell.mon.name, ev.pageX + 10, ev.pageY + 10);
    }


    $(document)
        .off("game_init.mouse_control")
        .on("game_init.mouse_control", function () {
            $("#dungeon")
                .on("cell_click", handle_cell_click)
                .on("cell_tooltip", handle_cell_tooltip);
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
