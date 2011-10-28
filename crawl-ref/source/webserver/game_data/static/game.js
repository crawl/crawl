define(["jquery", "comm", "client", "./dungeon_renderer", "./display", "./minimap",
        "./text"],
function ($, comm, client, dungeon_renderer, display, minimap) {
    var layout_parameters;

    function layout(data)
    {
        layout_parameters = data;

        do_layout();
    }

    function do_layout()
    {
        var window_width = $(window).width();
        var window_height = $(window).height();

        if (!layout_parameters)
            return false;

        var layer = current_layer;
        set_layer("normal");

        // Determine width/height of stats area
        var old_html = $("#stats").html();
        var s = "";
        for (var i = 0; i < layout_parameters.stat_width; i++)
            s = s + "&nbsp;";
        for (var i = 0; i < layout_parameters.min_stat_height; i++)
            s = s + "<br>";
        $("#stats").html(s);
        var stat_width_pixels = $("#stats").outerWidth();
        var stat_height_pixels = $("#stats").outerHeight();
        $("#stats").html(old_html);

        // Determine height of messages area
        old_html = $("#messages").html();
        s = "";
        for (var i = 0; i < layout_parameters.msg_min_height; i++)
            s = s + "<br>";
        $("#messages").html(s);
        var msg_height_pixels = $("#messages").outerHeight();
        $("#messages").html(old_html);

        var remaining_width = window_width - stat_width_pixels;
        var remaining_height = window_height - msg_height_pixels;

        // Determine the maximum size for the CRT layer
        set_layer("crt");
        old_html = $("#crt").html();
        var test_size = 20;
        s = "";
        for (var i = 0; i < test_size; i++)
            s = "&nbsp;" + s + "<br>";
        $("#crt").html(s);
        var char_w = $("#crt").width() / test_size;
        var char_h = $("#crt").height() / test_size;
        $("#crt").html(old_html);

        // Position controls
        set_layer("normal");
        dungeon_renderer.fit_to(remaining_width, remaining_height,
                                layout_parameters.show_diameter);

        minimap.fit_to(stat_width_pixels, layout_parameters);

        $("#monster_list").width(stat_width_pixels);

        // Go back to the old layer, re-hide the minimap if necessary
        set_layer(layer);

        // Update the view
        display.invalidate(true);
        display.display();
        minimap.update_overlay();
    }

    function handle_delay(data)
    {
        client.delay(data.t);
    }

    var game_version;
    function handle_version(data)
    {
        game_version = data;
        document.title = data.text;
    }

    comm.register_handlers({
        "layout": layout,
        "delay": handle_delay,
        "version": handle_version,
    });
});
