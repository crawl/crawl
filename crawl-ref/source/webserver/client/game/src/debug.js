define(["jquery", "client", "./dungeon_renderer", "./minimap",
        "./monster_list", "./map_knowledge", "./enums", "./display", "exports",
        "contrib/jquery.json"],
function ($, c, r, mm, ml, mk, enums, display, exports) {
    "use strict";

    exports.client = c;
    exports.renderer = r;
    exports.$ = $;
    exports.minimap = mm;
    exports.monster_list = ml;
    exports.map_knowledge = mk;
    exports.display = display;

    window.debug = exports;

    // Debug helper
    exports.mark_cell = function (x, y, mark)
    {
        mark = mark || "m";

        if (mk.get(x, y).t)
            mk.get(x, y).t.mark = mark;

        r.render_loc(x, y);
    }
    exports.unmark_cell = function (x, y)
    {
        var cell = mk.get(x, y);
        if (cell)
        {
            delete cell.t.mark;
        }

        r.render_loc(x, y);
    }
    exports.mark_all = function ()
    {
        var view = r.view;
        for (var x = 0; x < r.cols; x++)
            for (var y = 0; y < r.rows; y++)
                mark_cell(view.x + x, view.y + y, (view.x + x) + "/" + (view.y + y));
    }
    exports.unmark_all = function ()
    {
        var view = r.view;
        for (var x = 0; x < r.cols; x++)
            for (var y = 0; y < r.rows; y++)
                unmark_cell(view.x + x, view.y + y);
    }

    exports.obj_to_str = function (o)
    {
        return $.toJSON(o);
    }
});
