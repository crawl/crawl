map_knowledge = function ()
{
    var k = {};
    var monster_table = {};
    var next_monster_table = {};
    var dirty_locs = [];
    var bounds = null;
    var bounds_changed = false;

    var set = function (x, y, val)
    {
        k[[x,y]] = val;
    };

    var get = function (x, y)
    {
        if (k[[x,y]] === undefined)
            k[[x,y]] = {x: x, y: y};
        return k[[x,y]];
    };

    var clear = function ()
    {
        k = {};
        monster_table = {};
        bounds = null;
    };

    var touch = function (x, y)
    {
        var cell = get(x, y);
        if (!cell.dirty)
            dirty_locs.push({x: x, y: y});
        cell.dirty = true;
    };

    var merge_objects = function (current, diff)
    {
        if (!current)
            return diff;

        for (var prop in diff)
            current[prop] = diff[prop];

        return current;
    };

    var merge_monster = function (old_mon, mon)
    {
        if (!mon)
            return null;

        var id = mon.id;

        var last = monster_table[id];
        if (id === 0 || last == null)
        {
            if (old_mon)
                last = merge_objects(old_mon, mon);
            else
                last = mon;
        }
        else
        {
            merge_objects(last, mon);
        }
        
        if (id)
            next_monster_table[id] = last;

        return last;
    };

    var merge_last_x, merge_last_y;

    var merge = function (val)
    {
        var x;
        if (val.x === undefined)
            x = merge_last_x + 1;
        else
            x = val.x;
        if (val.y === undefined)
            y = merge_last_y;
        else
            y = val.y;
        merge_last_x = x;
        merge_last_y = y;

        var entry = get(x, y);
        
        for (var prop in val)
        {
            if (prop == "mon")
            {
                if (entry[prop] && entry[prop].id && !val[prop])
                    delete next_monster_table[entry[prop].id];
                
                entry[prop] = merge_monster(entry[prop], val[prop]);
            }
            else if (prop == "t")
            {
                entry[prop] = merge_objects(entry[prop], val[prop]);

                // The transparency flag is linked to the doll;
                // if the doll changes, it is reset
                if (val[prop].doll && val[prop].trans === undefined)
                    entry[prop].trans = false;
            }
            else
                entry[prop] = val[prop];
        }

        touch(x, y);
        
        if (bounds)
        {
            if (bounds.left > x)
            {
                bounds.left = x;
                bounds_changed = true;
            }
            if (bounds.right < x)
            {
                bounds.right = x;
                bounds_changed = true;
            }
            if (bounds.top > y)
            {
                bounds.top = y;
                bounds_changed = true;
            }
            if (bounds.bottom < y)
            {
                bounds.bottom = y;
                bounds_changed = true;
            }
        }
        else
        {
            bounds = {
                left: x,
                top: y,
                right: x,
                bottom: y
            };
        }

    };

    var merge_diff = function (vals)
    {
        next_monster_table = {};
        for (var id in monster_table)
            next_monster_table[id] = monster_table[id];

        $.each(vals, function (i, val)
               {
                   merge(val);
               });

        monster_table = next_monster_table;
    };

    return {
        set: set,
        get: get,
        merge: merge_diff,
        clear: clear,
        touch: touch,
        dirty: function () { return dirty_locs; },
        reset_dirty: function () { dirty_locs = []; },
        bounds: function () { return bounds; },
        reset_bounds_changed: function () {
            var bc = bounds_changed;
            bounds_changed = false;
            return bc;
        },
    };
} ();
