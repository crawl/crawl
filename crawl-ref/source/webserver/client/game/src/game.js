define(["jquery", "exports", "comm", "client", "key_conversion", "./dungeon_renderer",
        "./display", "./minimap", "./enums", "./messages", "./options",
        "./mouse_control", "./text", "./menu", "./action_panel",  "./player",
        "./ui","./ui-layouts"],
function ($, exports, comm, client, key_conversion, dungeon_renderer, display,
        minimap, enums, messages, options, mouse_control) {
    "use strict";

    var layout_parameters = null, ui_state, input_mode;
    var stat_width = 42;
    var msg_height;
    var show_diameter = 17;

    function setup_keycodes()
    {
        key_conversion.reset_keycodes();
        // the `codes` check and the else clause are here to prevent crashes
        // while players' caches get updated client.js and key_conversion.js
        // TODO: remove someday
        if (key_conversion.codes)
            key_conversion.enable_code_conversion();
        else
        {
            key_conversion.simple[96] = -1000; // numpad 0
            key_conversion.simple[97] = -1001; // numpad 1
            key_conversion.simple[98] = -1002; // numpad 2
            key_conversion.simple[99] = -1003; // numpad 3
            key_conversion.simple[100] = -1004; // numpad 4
            key_conversion.simple[101] = -1005; // numpad 5
            key_conversion.simple[102] = -1006; // numpad 6
            key_conversion.simple[103] = -1007; // numpad 7
            key_conversion.simple[104] = -1008; // numpad 8
            key_conversion.simple[105] = -1009; // numpad 9
            key_conversion.simple[106] = -1015; // numpad *
            key_conversion.simple[107] = -1016; // numpad +
            key_conversion.simple[108] = -1019; // numpad . (some firefox versions? TODO: confirm)
            key_conversion.simple[109] = -1018; // numpad -
            key_conversion.simple[110] = -1019; // numpad .
            key_conversion.simple[111] = -1012; // numpad /

            // fix some function key issues (see note in key_conversions.js).
            key_conversion.simple[112] = -265; // F1
            key_conversion.simple[113] = -266; // F2
            key_conversion.simple[114] = -267; // F3
            key_conversion.simple[115] = -268; // F4
            key_conversion.simple[116] = -269; // F5
            key_conversion.simple[117] = -270; // F6
            key_conversion.simple[118] = -271; // F7
            key_conversion.simple[119] = -272; // F8
            key_conversion.simple[120] = -273; // F9
            key_conversion.simple[121] = -274; // F10
            // reserve F11 for the browser (full screen)
            // reserve F12 for toggling chat
            key_conversion.simple[124] = -277; // F13, may not work
            // F13 may also show up as printscr, but we don't want to bind that
            key_conversion.simple[125] = -278; // F14, may not work?
            key_conversion.simple[126] = -279; // F15, may not work?
            key_conversion.simple[127] = -280;
            key_conversion.simple[128] = -281;
            key_conversion.simple[129] = -282;
            key_conversion.simple[130] = -283;
        }

        // any version-specific keycode overrides can be added here. (Though
        // hopefully this will be rarely needed in the future...)
    }

    function init()
    {
        layout_parameters = null;
        ui_state = -1;
        options.clear();
        setup_keycodes();
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
        $("#mobile_input input").width(remaining_width-12); // 2*padding+2*border

        // Go back to the old layer
        set_ui_state(state);

        // Update the view
        display.invalidate(true);
        display.display();
        minimap.update_overlay();

        var possible_input = $("#messages .game_message input");
        if (possible_input)
           possible_input.focus();

        // Input helper for mobile browsers
        // XX should this really happen in `layout`?
        if (!client.is_watching())
        {
            var mobile_input = options.get("tile_web_mobile_input_helper");
            if ((mobile_input === 'true') || (mobile_input === 'auto' && is_mobile()))
            {
                $("#mobile_input").show();
                $("#mobile_input input")
                    .off("keydown")
                    .on("keydown", handle_mobile_keydown)
                    .off("input")
                    .on("input", handle_mobile_input)
                    .off("mousedown focusout")
                    .on("mousedown", mobile_input_click);
                // the following requires a fairly modern browser
                $(document).on("visibilitychange",
                    function(ev)
                    {
                        // try to regularize the behavior, on iOS it's flaky
                        // otherwise
                        // bug: on iOS zooming out to view tabs doesn't seem
                        // to trip this event handler. Presumably because
                        // "prerendered" isn't yet implemented?
                        if (document.visibilityState !== "visible")
                            mobile_input_force_defocus();
                    });
            }
        }
    }

    options.add_listener(function () {
        minimap.init_options();
        if (layout_parameters)
            layout(layout_parameters, true);
        display.invalidate(true);
        display.display();
        glyph_mode_font_init();
        init_custom_text_colours();
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
            $(".action-panel").hide();

            dungeon_renderer.fit_to(width, height, show_diameter);
        }
        else
        {
            dungeon_renderer.fit_to(layout_parameters.remaining_width,
                                    layout_parameters.remaining_height,
                                    show_diameter);
            $("#right_column").show();
            $(".action-panel").show();
            messages.show();
        }
        minimap.stop_minimap_farview();
        minimap.update_overlay();
        display.invalidate(true);
        display.display();
    }

    function set_ui_state(state)
    {
        dungeon_renderer.set_ui_state(state);
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
            messages.message_pane_height = msg_height;
        }
        else
            msg_height = messages.message_pane_height;

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

    function handle_set_ui_cutoff(data)
    {
        var popups = document.querySelectorAll("#ui-stack > .ui-popup");
        Array.from(popups).forEach(function (p, i) {
            p.classList.toggle("hidden", i <= data.cutoff);
        });
    }

    function set_input_mode(mode)
    {
        if (mode == input_mode) return;
        input_mode = mode;
        dungeon_renderer.update_mouse_mode(input_mode);
        if (mode == enums.mouse_mode.COMMAND)
            messages.new_command();
    }

    game.get_input_mode = function() { return input_mode; }
    game.get_ui_state = function() { return ui_state; }

    game.can_target = function()
    {
        return (input_mode == enums.mouse_mode.TARGET
                || input_mode == enums.mouse_mode.TARGET_DIR
                || input_mode == enums.mouse_mode.TARGET_PATH);
    }
    game.can_move = function()
    {
        // XX just looking
        return (input_mode == enums.mouse_mode.COMMAND
                || ui_state == enums.ui.VIEW_MAP);
    }
    game.can_describe = function()
    {
        return (input_mode == enums.mouse_mode.TARGET
                || input_mode == enums.mouse_mode.TARGET_DIR
                || input_mode == enums.mouse_mode.TARGET_PATH
                || input_mode == enums.mouse_mode.COMMAND
                || ui_state == enums.ui.VIEW_MAP);
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

    function glyph_mode_font_init()
    {
        if (options.get("tile_display_mode") == "tiles") return;

        var glyph_font, glyph_size;
        glyph_size = options.get("glyph_mode_font_size");
        glyph_font = options.get("glyph_mode_font");

        if (!document.fonts.check(glyph_size + "px " + glyph_font))
            glyph_font = "monospace";

        var renderer_settings = {
            glyph_mode_font_size: glyph_size,
            glyph_mode_font: glyph_font
        };
        $.extend(dungeon_renderer, renderer_settings);
    }

    function init_custom_text_colours()
    {
        const root = document.querySelector(':root');

        // Reset colours first
        for (let i = 0; i < 16; i++)
            root.style.removeProperty('--color-' + i);

        // Load custom replacements
        var colours = options.get("custom_text_colours");
        for (var i in colours)
        {
            root.style.setProperty('--color-' + colours[i].index,
                "rgba(" + colours[i].r + ", "
                        + colours[i].g + ", "
                        + colours[i].b + ", 255)");
        }
    }

    function is_mobile()
    {
        return ('ontouchstart' in document.documentElement);
    }

    function handle_mobile_input(e)
    {
        e.target.value = e.target.defaultValue;
        comm.send_message("input", { text: e.originalEvent.data });
    }

    function handle_mobile_keydown(e)
    {
        // translate backspace/delete to esc -- the backspace key is almost
        // entirely unused outside of text input, and the lack of esc on a
        // mobile keyboard is really painful. Text input doesn't go through
        // this input, so that case is fine. (One minor case is the macro
        // edit menu, but there's at least an alternate way to clear a
        // binding.)
        if (e.which == 8 || e.which == 46)
        {
            comm.send_message("key", { keycode: 27 });
            e.preventDefault();
            return false;
        }
    }

    function mobile_input_focus_style(focused)
    {
        // manually manage the style so that it doesn't blink when the
        // focusout handler is doing its thing
        var mi = $("#mobile_input input");
        if (focused)
        {
            mi.attr("placeholder", "Tap here to close keyboard");
            mi.css("background", "rgba(100, 100, 100, 0.5)");
        }
        else
        {
            mi.attr("placeholder", "Tap here for keyboard input");
            mi.css("background", "rgba(0, 0, 0, 0.5)");
        }
    }

    function mobile_input_focused()
    {
        return $("#mobile_input input").is(":focus");
    }

    function mobile_input_focusout(ev)
    {
        var $mi = $("#mobile_input input");
        if (ev.relatedTarget && $(ev.relatedTarget).is("input"))
        {
            // ok, we'll allow it, but we want it back later. As long as focus
            // goes from input to input, the keyboard seems to stay open.
            $(ev.relatedTarget).off("focusout").on("focusout", function (ev)
                {
                    $mi[0].focus();
                    $mi.off("focusout").on("focusout", mobile_input_focusout);
                    mobile_input_focus_style(true);
                    $(ev.relatedTarget).off("focusout");
                });
            $mi.off("focusout");
            mobile_input_focus_style(false);
        }
        else
        {
            // force focus to stay on the mobile input. For iOS purposes, this
            // is the only thing I've tried that works. This *can't* happen in
            // a timeout, or the temporary loss of focus is enough to close
            // the keyboard.
            // Bug: tapping chat close hides the keyboard
            $mi[0].focus();
        }
    }

    function mobile_input_force_defocus()
    {
        var $mi = $("#mobile_input input");
        // remove any event handlers first
        $mi.off("focusout");
        mobile_input_focus_style(false);
        $mi.blur();
    }

    function mobile_input_click(ev)
    {
        var $mi = $("#mobile_input input");
        if (mobile_input_focused())
        {
            mobile_input_force_defocus();
            ev.preventDefault();
        }
        else
        {
            $mi.off("focusout").on("focusout", mobile_input_focusout);
            mobile_input_focus_style(true);
            $mi.focus();
            ev.preventDefault();
        }
    }

    $(document).ready(function () {
        $(window)
            .off("resize.game")
            .on("resize.game", function () {
            if (layout_parameters)
            {
                var params = $.extend({}, layout_parameters);
                layout(params);
            }
            $("#action-panel").triggerHandler("update");
        });
    });

    comm.register_handlers({
        "delay": handle_delay,
        "version": handle_version,
        "layout": handle_set_layout,
        "ui_state": handle_set_ui_state,
        "ui_cutoff": handle_set_ui_cutoff,
        "input_mode": handle_set_input_mode,
    });

    // ugly circular reference breaking
    mouse_control.game = game;
});
