monster_list = function ()
{
    var monsters = {};

    function update_loc(loc)
    {
        if (map_knowledge.visible(loc.x, loc.y))
        {
            var mon = map_knowledge.get(loc.x, loc.y).mon;
            if (mon)
                monsters[[loc.x,loc.y]] = mon;
            else
                delete monsters[[loc.x,loc.y]];
        }
        else
            delete monsters[[loc.x,loc.y]];
    }

    return {
        update_loc: update_loc,
        get: function () { return monsters; }
    };
} ();
