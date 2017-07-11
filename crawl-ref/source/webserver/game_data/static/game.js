define(["jquery", "comm", "client", "./dungeon_renderer", "./display",
        "./minimap", "./enums", "./messages", "./options", "./text", "./menu",
        "./player", "./mouse_control"],
function ($, comm, client, dungeon_renderer, display, minimap, enums, messages,
          options) {
    "use strict";

    var layout_parameters = null, ui_state, input_mode;
    var stat_width = 42;
    var msg_height;
    var show_diameter = 17;

    function init()
    {
        layout_parameters = null;
        ui_state = -1;
        options.clear();
    }

    $(document).on("game_preinit game_cleanup", init);

    function layout_params_differ(old_params, new_params)
    {
        if (!old_params) return true;
        for (var param in new_params)
        {
            if (old_params.hasOwnProperty(param) &&
                old_params[param] != new_params[param])
                return true;
        }
        return false;
    }

    function layout(params, force)
    {
        var window_width = params.window_width = $(window).width();
        var window_height = params.window_height = $(window).height();

        if (!force && !layout_params_differ(layout_parameters, params))
            return false;

        layout_parameters = params;

        var state = ui_state;
        set_ui_state(enums.ui.NORMAL);

        // Determine width of stats area
        var old_html = $("#stats").html();
        var s = "";
        for (var i = 0; i < enums.stat_width; i++)
            s = s + "&nbsp;";
        $("#stats").html(s);
        var stat_width_px = $("#stats").outerWidth();
        $("#stats").html(old_html);

        // Determine height of messages area

        // need to clone this, not copy html, since there can be event handlers
        // embedded in here on an input box.
        var old_messages = $("#messages").clone(true);
        var old_scroll_top = $("#messages_container").scrollTop();
        s = "";
        for (var i = 0; i < msg_height+1; i++)
            s = s + "<br>";
        $("#messages").html(s);
        var msg_height_px = $("#messages").outerHeight();
        $("#messages").replaceWith(old_messages);
        $("#messages_container").scrollTop(old_scroll_top);

        var remaining_width = window_width - stat_width_px;
        var remaining_height = window_height - msg_height_px;

        layout_parameters.remaining_width = remaining_width;
        layout_parameters.remaining_height = remaining_height;

        // Position controls
        client.set_layer("normal");
        $("#messages_container").css({
            "max-height": msg_height_px*msg_height/(msg_height+1),
            "width": remaining_width
        });
        dungeon_renderer.fit_to(remaining_width, remaining_height,
                                show_diameter);

        minimap.fit_to(stat_width_px, layout_parameters);

        $("#stats").width(stat_width_px);
        $("#monster_list").width(stat_width_px);

        // Go back to the old layer
        set_ui_state(state);

        // Update the view
        display.invalidate(true);
        display.display();
        minimap.update_overlay();

        var possible_input = $("#messages .game_message input");
        if (possible_input)
           possible_input.focus();
    }

    options.add_listener(function () {
        minimap.init_options();
        if (layout_parameters)
            layout(layout_parameters, true);
        display.invalidate(true);
        display.display();
    });

    function toggle_full_window_dungeon_view(full)
    {
        // Toggles the dungeon view for X map mode
        if (layout_parameters == null) return;
        if (full)
        {
            var width = layout_parameters.remaining_width;
            var height = layout_parameters.remaining_height;

            if (options.get("tile_level_map_hide_sidebar") === true) {
                width = layout_parameters.window_width - 5;
                $("#right_column").hide();
            }
            if (options.get("tile_level_map_hide_messages") === true) {
                height = layout_parameters.window_height - 5;
                messages.hide();
            }

            dungeon_renderer.fit_to(width, height, show_diameter);
        }
        else
        {
            dungeon_renderer.fit_to(layout_parameters.remaining_width,
                                    layout_parameters.remaining_height,
                                    show_diameter);
            $("#right_column").show();
            messages.show();
        }
        minimap.stop_minimap_farview();
        display.invalidate(true);
        display.display();
    }

    function set_ui_state(state)
    {
        if (state == ui_state) return;
        var old_state = ui_state;
        ui_state = state;
        switch (ui_state)
        {
        case enums.ui.NORMAL:
            client.set_layer("normal");
            if (old_state == enums.ui.VIEW_MAP)
                toggle_full_window_dungeon_view(false);
            break;

        case enums.ui.CRT:
            client.set_layer("crt");
            break;

        case enums.ui.VIEW_MAP:
            toggle_full_window_dungeon_view(true);
            break;
        }
    }

    function handle_set_layout(data)
    {
        if (data.message_pane.height)
        {
            msg_height = data.message_pane.height
                         + (data.message_pane.small_more ? 0 : -1);
        }

        if (layout_parameters == null)
            layout({});
        else
        {
            var params = $.extend({}, layout_parameters);
            layout(params, true);
        }
    }

    function handle_set_ui_state(data)
    {
        set_ui_state(data.state);
    }

    function set_input_mode(mode)
    {
        if (mode == input_mode) return;
        input_mode = mode;
        if (mode == enums.mouse_mode.COMMAND)
            messages.new_command();
    }

    function handle_set_input_mode(data)
    {
        set_input_mode(data.mode);
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

    var renderer_settings = {
        glyph_mode_font_size: 24,
        glyph_mode_font: "monospace"
    };

    $.extend(dungeon_renderer, renderer_settings);

    $(document).ready(function () {
        $(window)
            .off("resize.game")
            .on("resize.game", function () {
            if (layout_parameters)
            {
                var params = $.extend({}, layout_parameters);
                layout(params);
            }
        });
    });

    comm.register_handlers({
        "delay": handle_delay,
        "version": handle_version,
        "layout": handle_set_layout,
        "ui_state": handle_set_ui_state,
        "input_mode": handle_set_input_mode,
    });
});
