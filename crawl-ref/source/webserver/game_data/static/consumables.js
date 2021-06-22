define(["jquery", "./cell_renderer", "./enums", "./options", "./player",
        "./tileinfo-icons", "./util"],
function ($, cr, enums, options, player, icons, util) {
    "use strict";

    var renderer, $canvas, $settings;
    var borders_width;
    var minimized;
    // Options
    var scale, orientation, font_family, font_size;
    var font; // cached font name for the canvas: size (in px) + family

    // Initial setup for the panel and its settings menu.
    // Note that "game_init" happens before the client receives
    // the options and inventory data from the server.
    $(document).bind("game_init", function () {
        $canvas = $("#consumables");
        renderer = new cr.DungeonCellRenderer();
        borders_width = (parseInt($canvas.css("border-left-width"), 10) || 0) * 2;
        minimized = false;

        $canvas.on("update", update);

        $settings = $("#consumables-settings");
        $canvas.contextmenu(function (e) {
            // Initialize the form with the current values
            $("#orient-" + orientation).prop("checked", true);

            // Parsing is required, because 1.1 * 100 is 110.00000000000001
            // instead of 110 in JavaScript
            var scale_percent = parseInt(scale * 100, 10);
            $("#scale-val").val(scale_percent);
            if (!$("#scale-val").data("default"))
                $("#scale-val").data("default", scale_percent);

            $("#font-size-val").val(font_size);
            if (!$("#font-size-val").data("default"))
                $("#font-size-val").data("default", font_size);

            // Show the context menu near the cursor
            $settings.css({top: e.pageY + 10 + "px",
                          left: e.pageX + 10 + "px"});
            $settings.show();

            return false;
        });

        // We don't need a context menu for the context menu
        $settings.contextmenu(function () {
            return false;
        });

        // Clicking on the panel/Close button closes the settings menu
        $("#consumables, #close-settings").click(function () {
            $settings.hide();
        });

        // Triggering this function on keyup might be too agressive,
        // but at least the player doesn't have to press Enter to confirm changes
        $("#consumables-settings input[type=radio],input[type=number]")
            .on("change keyup", function (e) {
                var input = e.target;
                if (input.type === "number" && !input.checkValidity())
                    return;
                window.set_option(input.name, input.value);
        });

        $("#consumables-settings button.reset").click(function () {
            var input = $(this).siblings("input");
            var default_value = input.data("default");
            input.val(default_value);
            window.set_option(input.prop("name"), default_value);
        });

        $("#minimize-panel").click(function () {
            $settings.hide();
            $canvas.addClass("hidden");
            $("#consumables-placeholder").removeClass("hidden").show();
            minimized = true;
        });

        $("#consumables-placeholder").click(function () {
            $("#consumables-placeholder").addClass("hidden");
            $canvas.removeClass("hidden");
            minimized = false;
            update();
        });

        // To prevent the game from showing an empty panel before
        // any inventory data arrives, we hide it via inline CSS
        // and the "hidden" class. The next line deactivates
        // the inline rule, and the first call to update() will
        // remove "hidden" if the (filtered) inventory is not empty.
        $canvas.show();
    });

    function _horizontal()
    {
        return orientation === "horizontal" ? true : false;
    }

    function _update_font_props()
    {
        font = (font_size || "16") + "px " + (font_family || "monospace");
    }

    function update()
    {
        if (minimized)
            return;

        // Filter
        var filtered_inv = Object.values(player.inv).filter(function (item) {
            if (!item.quantity) // Skip empty inventory slots
                return false
            else if (item.hasOwnProperty("qty_field") && item.qty_field)
                return true;
        });

        if (!filtered_inv.length)
        {
            $canvas.addClass("hidden");
            return;
        }

        // Sort
        filtered_inv.sort(function (a, b) {
            if (a.base_type === b.base_type)
                return a.sub_type - b.sub_type;

            return a.base_type - b.base_type;
        });

        // Render
        var cell_width = renderer.cell_width * scale;
        var cell_height = renderer.cell_height * scale;
        var cell_length = _horizontal() ? cell_width
                                        : cell_height;
        var required_length = cell_length * filtered_inv.length;
        var available_length = _horizontal() ? $("#dungeon").width()
                                             : $("#dungeon").height();
        available_length -= borders_width;
        var max_cells = Math.floor(available_length / cell_length);
        var panel_length = Math.min(required_length, available_length);

        util.init_canvas($canvas[0],
                         _horizontal() ? panel_length : cell_width,
                         _horizontal() ? cell_height : panel_length);
        renderer.init($canvas[0]);

        renderer.ctx.fillStyle = "black";
        renderer.ctx.fillRect(0, 0,
                              _horizontal() ? panel_length : cell_width,
                              _horizontal() ? cell_height : panel_length);

        filtered_inv.slice(0, max_cells).forEach(function (item, idx) {
            var offset = cell_length * idx;
            item.tile.forEach(function (tile) { // Draw item and brand tiles
                renderer.draw_main(tile,
                                   _horizontal() ? offset : 0,
                                   _horizontal() ? 0 : offset,
                                   scale);
            });

            var qty_field_name = item.qty_field;
            if (item.hasOwnProperty(qty_field_name))
            {
                renderer.draw_quantity(item[qty_field_name],
                                       _horizontal() ? offset : 0,
                                       _horizontal() ? 0 : offset,
                                       font);
            }
        });

        if (available_length < required_length)
        {
            var ellipsis = icons.ELLIPSIS;
            var x_pos = 0, y_pos = 0;

            if (_horizontal())
                x_pos = available_length - icons.get_tile_info(ellipsis).w * scale;
            else
                y_pos = available_length - icons.get_tile_info(ellipsis).h * scale;

            renderer.draw_icon(ellipsis, x_pos, y_pos, -2, -2, scale);
        }
        $canvas.removeClass("hidden");
    }

    options.add_listener(function () {
        var update_required = false;

        var new_scale = options.get("consumables_panel_scale") / 100;
        if (scale !== new_scale)
        {
            scale = new_scale;
            update_required = true;
        }

        var new_orientation = options.get("consumables_panel_orientation");
        if (orientation !== new_orientation)
        {
            orientation = new_orientation;
            update_required = true;
        }

        var new_font_family = options.get("consumables_panel_font_family");
        if (font_family !== new_font_family)
        {
            font_family = new_font_family;
            _update_font_props();
            update_required = true;
        }

        var new_font_size = options.get("consumables_panel_font_size");
        if (font_size !== new_font_size)
        {
            font_size = new_font_size;
            _update_font_props();
            update_required = true;
        }

        if (update_required)
            update();
    });
});
