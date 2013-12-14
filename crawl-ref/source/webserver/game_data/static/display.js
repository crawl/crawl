define(["jquery", "comm", "./map_knowledge", "./view_data", "./monster_list",
        "./minimap", "./dungeon_renderer"],
function ($, comm, map_knowledge, view_data, monster_list, minimap,
          dungeon_renderer) {
    "use strict";

    var overlaid_locs = [];

    function invalidate(minimap_too)
    {
        var b = map_knowledge.bounds();
        if (!b) return;

        var view = dungeon_renderer.view;

        var xs = minimap_too ? b.left : view.x;
        var ys = minimap_too ? b.top : view.y;
        var xe = minimap_too ? b.right : view.x + dungeon_renderer.cols - 1;
        var ye = minimap_too ? b.bottom : view.y + dungeon_renderer.rows - 1;
        for (var x = xs; x <= xe; x++)
            for (var y = ys; y <= ye; y++)
        {
            map_knowledge.touch(x, y);
        }
    }

    function display()
    {
        // Update the display.
        if (!map_knowledge.bounds())
            return;

        var t1 = new Date();

        if (map_knowledge.reset_bounds_changed())
        {
            minimap.center();
        }

        if (view_data.flash_changed())
        {
            invalidate();
        }

        var dirty_locs = map_knowledge.dirty();
        for (var i = 0; i < dirty_locs.length; i++)
        {
            var loc = dirty_locs[i];
            var cell = map_knowledge.get(loc.x, loc.y);
            cell.dirty = false;
            monster_list.update_loc(loc);
            dungeon_renderer.render_loc(loc.x, loc.y, cell);
            minimap.update(loc.x, loc.y, cell);
        }
        map_knowledge.reset_dirty();

        dungeon_renderer.animate();

        monster_list.update();

        var render_time = (new Date() - t1);
        if (!window.render_times)
            window.render_times = [];
        if (window.render_times.length > 20)
            window.render_times.shift();
        window.render_times.push(render_time);
    }

    function clear_map()
    {
        map_knowledge.clear();

        dungeon_renderer.clear();

        minimap.clear();

        monster_list.clear();
    }

    // Message handlers
    function handle_map_message(data)
    {
        if (data.clear)
            clear_map();

        if (data.player_on_level != null)
            map_knowledge.set_player_on_level(data.player_on_level);

        if (data.vgrdc)
            minimap.do_view_center_update(data.vgrdc.x, data.vgrdc.y);

        if (data.cells)
            map_knowledge.merge(data.cells);

        // Mark cells above high cells as dirty
        $.each(map_knowledge.dirty().slice(), function (i, loc) {
            var cell = map_knowledge.get(loc.x, loc.y);
            if (cell.t && cell.t.sy && cell.t.sy < 0)
                map_knowledge.touch(loc.x, loc.y - 1);
        });

        display();
    }

    function handle_overlay_message(data)
    {
        dungeon_renderer.draw_overlay(data.idx, data.x, data.y);
        overlaid_locs.push(data);
    }

    function clear_overlays()
    {
        $("#dungeon").trigger("update_cells", [overlaid_locs]);
        overlaid_locs = [];
    }

    function handle_vgrdc(data)
    {
    }

    comm.register_handlers({
        "map": handle_map_message,
        "overlay": handle_overlay_message,
        "clear_overlays": clear_overlays,
    });


    return {
        invalidate: invalidate,
        display: display,
    };
});
