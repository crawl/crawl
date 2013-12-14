define(["jquery", "./enums"], function ($, enums) {
    "use strict";

    var k, player_on_level, monster_table, dirty_locs, bounds, bounds_changed;

    function init()
    {
        k = {};
        monster_table = {};
        dirty_locs = [];
        bounds = null;
        bounds_changed = false;
    }

    $(document).bind("game_init", init);

    function set(x, y, val)
    {
        k[[x,y]] = val;
    }

    function get(x, y)
    {
        if (k[[x,y]] === undefined)
            k[[x,y]] = {x: x, y: y};
        return k[[x,y]];
    }

    function clear()
    {
        k = {};
        monster_table = {};
        bounds = null;
    }

    function visible(x, y)
    {
        var cell = get(x, y);
        if (cell.t)
        {
            cell.t.bg = enums.prepare_bg_flags(cell.t.bg || 0);
            return !cell.t.bg.UNSEEN;
        }
        return false;
    }

    function touch(x, y)
    {
        var pos = (y === undefined) ? x : {x: x, y: y};
        var cell = get(pos.x, pos.y);
        if (!cell.dirty)
            dirty_locs.push(pos);
        cell.dirty = true;
    }

    function merge_objects(current, diff)
    {
        if (!current)
            return diff;

        for (var prop in diff)
            current[prop] = diff[prop];

        return current;
    }

    function set_monster_defaults(mon)
    {
        mon.att = mon.att || 0;
    }

    function merge_monster(old_mon, mon)
    {
        if (old_mon && old_mon.refs)
        {
            old_mon.refs--;
        }

        if (!mon)
        {
            return null;
        }

        var id = mon.id;

        var last = monster_table[id];
        if (!last)
        {
            if (old_mon)
                last = merge_objects(merge_objects({}, old_mon), mon);
            else
            {
                last = mon;
                set_monster_defaults(last);
            }
        }
        else
        {
            merge_objects(last, mon);
        }

        if (id)
        {
            last.refs = last.refs || 0;
            last.refs++;
            monster_table[id] = last;
        }

        return last;
    }

    function clean_monster_table()
    {
        for (var id in monster_table)
        {
            if (!monster_table[id].refs)
                delete monster_table[id];
        }
    }

    var merge_last_x, merge_last_y;

    function merge(val)
    {
        if (val === undefined) return;

        var x, y;
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

    }

    function merge_diff(vals)
    {
        $.each(vals, function (i, val)
               {
                   merge(val);
               });

        clean_monster_table();
    };

    return {
        set: set,
        get: get,
        merge: merge_diff,
        clear: clear,
        touch: touch,
        visible: visible,
        player_on_level: function () { return player_on_level; },
        set_player_on_level: function (v) { player_on_level = v; },
        dirty: function () { return dirty_locs; },
        reset_dirty: function () { dirty_locs = []; },
        bounds: function () { return bounds; },
        reset_bounds_changed: function () {
            var bc = bounds_changed;
            bounds_changed = false;
            return bc;
        },
    };
});
