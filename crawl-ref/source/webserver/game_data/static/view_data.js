define(["jquery", "comm"], function ($, comm) {
    "use strict";

    var exports = {};

    function VColour(r, g, b, a)
    {
        return {r: r, g: g, b: b, a: a};
    }

    // Compare tilereg-dgn.cc
    var flash_colours = [
        VColour(  0,   0,   0,   0), // BLACK (transparent)
        VColour(  0,   0, 128, 100), // BLUE
        VColour(  0, 128,   0, 100), // GREEN
        VColour(  0, 128, 128, 100), // CYAN
        VColour(128,   0,   0, 100), // RED
        VColour(150,   0, 150, 100), // MAGENTA
        VColour(165,  91,   0, 100), // BROWN
        VColour( 50,  50,  50, 150), // LIGHTGRAY
        VColour(  0,   0,   0, 150), // DARKGRAY
        VColour( 64,  64, 255, 100), // LIGHTBLUE
        VColour( 64, 255,  64, 100), // LIGHTGREEN
        VColour(  0, 255, 255, 100), // LIGHTCYAN
        VColour(255,  64,  64, 100), // LIGHTRED
        VColour(255,  64, 255, 100), // LIGHTMAGENTA
        VColour(150, 150,   0, 100), // YELLOW
        VColour(255, 255, 255, 100), // WHITE
    ];

    exports.get_flash_colour = function(flash_colour, flash_alpha)
    {
        if (!flash_colour)
            return null;
        const colour = flash_colours[flash_colour];
        //if (flash_alpha === 0)
            return colour;
        //return VColour(colour.r, colour.g, colour.b, flash_alpha);
    }

    exports.cursor_locs = [];
    function place_cursor(type, loc)
    {
        var old_loc;
        if (exports.cursor_locs[type])
        {
            old_loc = exports.cursor_locs[type];
        }

        exports.cursor_locs[type] = loc;

        var rerender = [];

        if (old_loc)
            rerender.push(old_loc);

        if (loc)
            rerender.push(loc);

        $("#dungeon").trigger("update_cells", [rerender]);
    }

    exports.place_cursor = place_cursor;
    exports.remove_cursor = function (type)
    {
        place_cursor(type, null);
    }

    function handle_cursor_message(data)
    {
        place_cursor(data.id, data.loc);
    }

    function init()
    {
        exports.cursor_locs = [];
    }

    $(document).bind("game_init", init);

    comm.register_handlers({
        "cursor": handle_cursor_message,
    });

    return exports;
});
