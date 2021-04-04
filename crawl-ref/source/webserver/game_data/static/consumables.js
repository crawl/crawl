define(["jquery", "./cell_renderer", "./enums", "./options", "./player",
        "./tileinfo-icons", "./util"],
function ($, cr, enums, options, player, icons, util) {
    "use strict";

    var renderer, $canvas;
    var borders_width;
    // Options
    var base_types = [], filters = [], show_unidentified, scale;
    var filters_text = "";

    $(document).bind("game_init", function () {
        $canvas = $("#consumables");
        renderer = new cr.DungeonCellRenderer();
        $canvas.on("update", update);
        borders_width = (parseInt($canvas.css("border-left-width"), 10) || 0)
                        + (parseInt($canvas.css("border-right-width"), 10) || 0);
    });

    function update()
    {
        if (!base_types.length)
        {
            $canvas.addClass("empty");
            return;
        }

        // Filter
        var filtered_inv = Object.values(player.inv).filter(function(item) {
            if (!item.quantity
                || !base_types.includes(item.base_type)
                || (!show_unidentified && !item.flags)
                || filters.some(function(re) { return item.name.match(re); }))
            {
                return false;
            }
            else
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

            var qty;

            if (i.base_type === enums.base_type.OBJ_MISCELLANY
                && i.sub_type !== enums.misc_item_type.MISC_ZIGGURAT)
            {
                qty = i.plus;
            }
            else
                qty = i.plus || i.quantity;

            renderer.draw_quantity(qty, renderer.cell_width * idx * scale, 0, scale);
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
        var update_required = false;

        var new_base_types = options.get("consumables_panel");
        if (base_types.toString() !== new_base_types.toString())
        {
            base_types = new_base_types;
            update_required = true;
        }

        var new_filters = options.get("consumables_panel_filter");
        if (filters_text !== new_filters.toString())
        {
            filters_text = new_filters.toString();

            filters = [];
            new_filters.forEach(function(pattern) {
                filters.push(new RegExp(pattern));
            });

            update_required = true;
        }

        var new_show_unid = options.get("show_unidentified_consumables");
        if (show_unidentified !== new_show_unid)
        {
            show_unidentified = new_show_unid;
            update_required = true;
        }

        var new_scale = options.get("consumables_panel_scale") / 100;
        if (scale !== new_scale)
        {
            scale = new_scale;
            update_required = true;
        }

        if (update_required)
            update();
    });
});
