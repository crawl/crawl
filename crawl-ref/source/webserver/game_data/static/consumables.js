define(["jquery", "./cell_renderer", "./enums", "./options", "./player",
        "./tileinfo-icons", "./util"],
function ($, cr, enums, options, player, icons, util) {
    "use strict";

    var renderer, $canvas;
    var borders_width;
    // Options
    var scale, orientation, font;

    $(document).bind("game_init", function () {
        $canvas = $("#consumables");
        renderer = new cr.DungeonCellRenderer();
        borders_width = (parseInt($canvas.css("border-left-width"), 10) || 0) * 2;

        $canvas.on("update", update);
    });

    function _horizontal()
    {
        return orientation === "horizontal" ? true : false;
    }

    function update()
    {
        // Filter
        var filtered_inv = Object.values(player.inv).filter(function(item) {
            if (!item.quantity) // Skip empty inventory slots
                return false
            else if (item.hasOwnProperty("qty_field") && item.qty_field)
                return true;
        });

        if (!filtered_inv.length)
        {
            $canvas.addClass("empty");
            return;
        }

        // Sort
        filtered_inv.sort(function(a, b) {
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

        filtered_inv.slice(0, max_cells).forEach(function(item, idx) {
            var offset = cell_length * idx;
            item.tile.forEach(function(tile) { // Draw item and brand tiles
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
        $canvas.removeClass("empty");
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

        var new_font = options.get("consumables_panel_font");
        if (font !== new_font)
        {
            font = new_font;
            update_required = true;
        }

        if (update_required)
            update();
    });
});
