define(["jquery", "./map_knowledge", "./dungeon_renderer", "./view_data",
        "./tileinfo-player", "./tileinfo-main", "./tileinfo-dngn", "./enums",
        "./player", "./options", "./util"],
function ($, map_knowledge, dungeon_renderer, view_data,
          tileinfo_player, main, dngn, enums, player, options, util) {
    "use strict";

    var minimap_colours;
    var overlay;
    var ctx, overlay_ctx;
    var cell_w, cell_h;
    var cell_x = 0, cell_y = 0;
    var display_x = 0, display_y = 0;
    var enabled = true;

    function vcolour_to_css_colour(colour)
    {
        return "rgba(" + colour.r + "," + colour.g + "," + colour.b + ","
               + (colour.a ? colour.a : 255) + ")";
    }

    function update_overlay()
    {
        // Update the minimap overlay
        var view = dungeon_renderer.view;
        overlay_ctx.clearRect(0, 0, overlay.width, overlay.height);
        overlay_ctx.strokeStyle =
            vcolour_to_css_colour(options.get("tile_window_col"));
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
        // suppress rclick menu (so we can rclick to view)
        $("#minimap")[0].oncontextmenu =  function() { return false; }
        overlay.oncontextmenu =  function() { return false; }
    }

    function clear()
    {
        ctx.fillStyle = "black";
        ctx.fillRect(0, 0,
                     $("#minimap").width(),
                     $("#minimap").height());
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

    function init_options()
    {
        minimap_colours = [
            options.get("tile_unseen_col"),         // MF_UNSEEN
            options.get("tile_floor_col"),          // MF_FLOOR
            options.get("tile_wall_col"),           // MF_WALL
            options.get("tile_mapped_floor_col"),   // MF_MAP_FLOOR
            options.get("tile_mapped_wall_col"),    // MF_MAP_WALL
            options.get("tile_door_col"),           // MF_DOOR
            options.get("tile_item_col"),           // MF_ITEM
            options.get("tile_monster_col"),        // MF_MONS_FRIENDLY
            options.get("tile_monster_col"),        // MF_MONS_PEACEFUL
            options.get("tile_monster_col"),        // MF_MONS_NEUTRAL
            options.get("tile_monster_col"),        // MF_MONS_HOSTILE
            options.get("tile_plant_col"),          // MF_MONS_NO_EXP
            options.get("tile_upstairs_col"),       // MF_STAIR_UP
            options.get("tile_transporter_col"),    // MF_TRANSPORTER
            options.get("tile_downstairs_col"),     // MF_STAIR_DOWN
            options.get("tile_branchstairs_col"),   // MF_STAIR_BRANCH
            options.get("tile_feature_col"),        // MF_FEATURE
            options.get("tile_water_col"),          // MF_WATER
            options.get("tile_lava_col"),           // MF_LAVA
            options.get("tile_trap_col"),           // MF_TRAP
            options.get("tile_excl_centre_col"),    // MF_EXCL_ROOT
            options.get("tile_excluded_col"),       // MF_EXCL
            options.get("tile_player_col"),         // MF_PLAYER
            options.get("tile_deep_water_col"),     // MF_DEEP_WATER
            options.get("tile_portal_col")          // MF_PORTAL
        ].map(vcolour_to_css_colour);
    }

    function update(x, y, cell)
    {
        cell = cell || map_knowledge.get(x, y);
        if (x == player.pos.x && y == player.pos.y)
            set(x, y, minimap_colours[enums.MF_PLAYER]);
        else
            set(x, y, minimap_colours[cell.mf || enums.MF_UNSEEN]);
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
    });

    return {
        init_options: init_options,
        fit_to: fit_to,
        update_overlay: update_overlay,
        clear: clear,
        center: center,
        update: update,
        do_view_center_update: do_view_center_update,
        stop_minimap_farview: stop_minimap_farview,
    }
});
