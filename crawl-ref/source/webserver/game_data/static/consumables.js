define(["jquery", "./cell_renderer", "./enums", "./options", "./player",
        "./tileinfo-icons", "./util"],
function ($, cr, enums, options, player, icons, util) {
    "use strict";

    var renderer, $canvas;
    var borders_width;
    var scale;

    $(document).bind("game_init", function () {
        $canvas = $("#consumables");
        renderer = new cr.DungeonCellRenderer();
        $canvas.on("update", update);
        borders_width = (parseInt($canvas.css("border-left-width"), 10) || 0)
                        + (parseInt($canvas.css("border-right-width"), 10) || 0);
    });

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
        var required_width = renderer.cell_width * filtered_inv.length * scale;
        var available_width = $("#dungeon").width() - borders_width;
        var max_cells = Math.floor(available_width / (renderer.cell_width * scale));
        var panel_width = Math.min(required_width, available_width);

        util.init_canvas($canvas[0], panel_width, renderer.cell_height * scale);
        renderer.init($canvas[0]);

        renderer.ctx.fillStyle = "black";
        renderer.ctx.fillRect(0, 0, panel_width, renderer.cell_height * scale);

        filtered_inv.slice(0, max_cells).forEach(function(i, idx) {
            i.tile.forEach(function(t) {
                renderer.draw_main(t, renderer.cell_width * idx * scale, 0, scale);
            });

            var qty_field_name = i.qty_field;
            if (i.hasOwnProperty(qty_field_name))
                renderer.draw_quantity(i[qty_field_name],
                                       renderer.cell_width * idx * scale, 0, scale);
        });

        if (available_width < required_width)
        {
            var ellipsis = icons.ELLIPSIS;
            var x_pos = available_width - icons.get_tile_info(ellipsis).w * scale;
            renderer.draw_icon(ellipsis, x_pos, 0, -2, -2, scale);
        }
        $canvas.removeClass("empty");
    }

    options.add_listener(function () {
        var new_scale = options.get("consumables_panel_scale") / 100;
        if (scale !== new_scale)
        {
            scale = new_scale;
            update();
        }
    });
});
