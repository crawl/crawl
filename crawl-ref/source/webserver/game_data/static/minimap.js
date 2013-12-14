define(["jquery", "./map_knowledge", "./dungeon_renderer", "./view_data",
        "./tileinfo-player", "./tileinfo-main", "./tileinfo-dngn", "./enums",
        "./player", "./options"],
function ($, map_knowledge, dungeon_renderer, view_data,
          tileinfo_player, main, dngn, enums, player, options) {
    "use strict";

    var minimap_colours = [
        "black",       // MF_UNSEEN
        "darkgrey",    // MF_FLOOR
        "grey",        // MF_WALL
        "darkgrey",    // MF_MAP_FLOOR
        "blue",        // MF_MAP_WALL
        "brown",       // MF_DOOR
        "green",       // MF_ITEM
        "#EE9090",     // MF_MONS_FRIENDLY
        "#EE9090",     // MF_MONS_PEACEFUL
        "red",         // MF_MONS_NEUTRAL
        "red",         // MF_MONS_HOSTILE
        "darkgreen",   // MF_MONS_NO_EXP
        "blue",        // MF_STAIR_UP
        "magenta",     // MF_STAIR_DOWN
        "cyan",        // MF_STAIR_BRANCH
        "cyan",        // MF_FEATURE
        "grey",        // MF_WATER
        "grey",        // MF_LAVA
        "yellow",      // MF_TRAP
        "darkblue",    // MF_EXCL_ROOT
        "darkcyan",    // MF_EXCL
        "white"        // MF_PLAYER
    ];

    var overlay;
    var ctx, overlay_ctx;
    var cell_w, cell_h;
    var cell_x = 0, cell_y = 0;
    var display_x = 0, display_y = 0;
    var enabled = true;

    function update_overlay()
    {
        // Update the minimap overlay
        var view = dungeon_renderer.view;
        overlay_ctx.clearRect(0, 0, overlay.width, overlay.height);
        overlay_ctx.strokeStyle = "yellow";
        overlay_ctx.strokeRect(display_x + (view.x - cell_x) * cell_w + 0.5,
                               display_y + (view.y - cell_y) * cell_h + 0.5,
                               dungeon_renderer.cols * cell_w - 1,
                               dungeon_renderer.rows * cell_h - 1);
    }

    function fit_to(width)
    {
        var gxm = enums.gxm;
        var gym = enums.gym;
        var display = $("#minimap").css("display");
        $("#minimap, #minimap_overlay").show();

        var block = $("#minimap_block");
        var canvas = $("#minimap")[0];
        if (!canvas) return;

        block.width(width);
        canvas.width = width;
        cell_w = cell_h = Math.floor(width / gxm);
        if (options.get("tile_map_pixels") > 0)
            cell_w = cell_h = Math.min(cell_w, options.get("tile_map_pixels"));
        block.height(gym * cell_h);
        canvas.height = gym * cell_h;
        ctx = canvas.getContext("2d");

        overlay = $("#minimap_overlay")[0];
        overlay.width = canvas.width;
        overlay.height = canvas.height;
        overlay_ctx = overlay.getContext("2d");

        if (cell_x !== 0 || cell_y !== 0)
        {
            clear();
            center();
        }

        $("#minimap, #minimap_overlay").css("display", display);
    }

    function clear()
    {
        ctx.fillStyle = "black";
        ctx.fillRect(0, 0,
                     $("#minimap").width(),
                     $("#minimap").height());
    }

    function get_cell_map_feature(map_cell)
    {
        var cell = map_cell.t;

        if (cell) cell.fg = enums.prepare_fg_flags(cell.fg || 0);

        if (cell && cell.fg.value == tileinfo_player.PLAYER)
            return enums.MF_PLAYER;
        else
            return map_cell.mf || enums.MF_UNSEEN;
    }

    function center()
    {
        if (!enabled) return;

        stop_minimap_farview();

        var minimap = $("#minimap")[0];
        var bounds = map_knowledge.bounds();
        var mm_w = bounds.right - bounds.left;
        var mm_h = bounds.bottom - bounds.top;

        if (mm_w < 0 || mm_h < 0) return;

        var old_x = display_x - cell_x * cell_w;
        var old_y = display_y - cell_y * cell_h;

        cell_x = bounds.left;
        cell_y = bounds.top;
        display_x = Math.floor((minimap.width - cell_w * mm_w) / 2);
        display_y = Math.floor((minimap.height - cell_h * mm_h) / 2);

        var dx = display_x - cell_x * cell_w - old_x;
        var dy = display_y - cell_y * cell_h - old_y;

        ctx.drawImage(minimap, 0, 0, minimap.width, minimap.height,
                               dx, dy, minimap.width, minimap.height);

        ctx.fillStyle = "black";
        if (dx > 0)
            ctx.fillRect(0, 0, dx, minimap.height);
        if (dx < 0)
            ctx.fillRect(minimap.width + dx, 0, -dx, minimap.height);
        if (dy > 0)
            ctx.fillRect(0, 0, minimap.width, dy);
        if (dy < 0)
            ctx.fillRect(0, minimap.height + dy, minimap.width, -dy);

        update_overlay();
    }

    function set(cx, cy, colour)
    {
        if (!enabled || colour === undefined) return;

        var x = cx - cell_x;
        var y = cy - cell_y;
        ctx.fillStyle = colour;
        ctx.fillRect(display_x + x * cell_w,
                     display_y + y * cell_h,
                     cell_w, cell_h);
    }

    function update(x, y, cell)
    {
        cell = cell || map_knowledge.get(x, y);
        var feat = get_cell_map_feature(cell);
        if (x == player.pos.x && y == player.pos.y)
            feat = enums.MF_PLAYER;
        set(x, y, minimap_colours[feat]);
    }

    // Minimap controls ------------------------------------------------------------
    function vgrdc(x, y)
    {
        dungeon_renderer.set_view_center(x, y);

        update_overlay();
    }

    function do_view_center_update(x, y)
    {
        // Update the viewpoint, unless the user is currently
        // right-clicking on the map
        if (farview_old_vc)
        {
            farview_old_vc.x = x;
            farview_old_vc.y = y;
        }
        else
        {
            vgrdc(x, y);
        }
    }

    var farview_old_vc;
    function minimap_farview(ev)
    {
        if (ev.which == 3 || farview_old_vc)
        {
            var offset = $("#minimap").offset();
            if (farview_old_vc === undefined)
            {
                farview_old_vc = {
                    x: dungeon_renderer.view_center.x,
                    y: dungeon_renderer.view_center.y
                }
            }
            var x = Math.round((ev.pageX - display_x - offset.left)
                               / cell_w + cell_x - 0.5);
            var y = Math.round((ev.pageY - display_y - offset.top)
                               / cell_h + cell_y - 0.5);
            vgrdc(x, y);
        }
    }

    function stop_minimap_farview()
    {
        if (farview_old_vc !== undefined)
        {
            vgrdc(farview_old_vc.x, farview_old_vc.y);
            farview_old_vc = undefined;
        }
    }

    $(document).bind("game_init", function () {
        cell_x = cell_y = display_x = display_y = 0;

        $("#minimap_overlay")
            .mousedown(minimap_farview)
            .mousemove(minimap_farview)
            .mouseup(stop_minimap_farview)
            .bind("contextmenu", function(ev) { ev.preventDefault(); });
    });

    return {
        fit_to: fit_to,
        update_overlay: update_overlay,
        clear: clear,
        center: center,
        update: update,
        do_view_center_update: do_view_center_update,
        stop_minimap_farview: stop_minimap_farview,
    }
});
