define(["jquery", "./map_knowledge", "./cell_renderer", "./dungeon_renderer"],
function ($, map_knowledge, cr, dungeon_renderer) {
    var monsters, $list, monster_groups, max_rows;

    function init()
    {
        monsters = {};
        $list = $("#monster_list");
        monster_groups = [];
        max_rows = 5;
    }
    $(document).bind("game_init", init);

    function update_loc(loc)
    {
        if (map_knowledge.visible(loc.x, loc.y))
        {
            var mon = map_knowledge.get(loc.x, loc.y).mon;
            if (mon)
                monsters[[loc.x,loc.y]] = { mon: mon, loc: loc };
            else
                delete monsters[[loc.x,loc.y]];
        }
        else
            delete monsters[[loc.x,loc.y]];
    }

    function can_combine(monster1, monster2)
    {
        return (monster_sort(monster1, monster2) == 0);
    }

    function is_excluded(monster)
    {
        return (monster.typedata.no_exp &&
                !(monster.name == "active ballistomycete"
                  || monster.name.match(/tentacle$/)));
    }

    function monster_sort(m1, m2)
    {
        // Compare monster_info::less_than
        if (m1.att < m2.att)
            return -1;
        else if (m1.att > m2.att)
            return 1;

        if (m1.typedata.avghp > m2.typedata.avghp)
            return -1;
        else if (m1.typedata.avghp < m2.typedata.avghp)
            return 1;

        if (m1.type < m2.type)
            return 1;
        else if (m1.type > m2.type)
            return -1;

        if (m1.name < m2.name)
            return 1;
        else if (m1.name > m2.name)
            return -1;

        return 0;
    }

    function group_monsters()
    {
        var monster_list = [];

        for (var loc in monsters)
        {
            if (is_excluded(monsters[loc].mon)) continue;
            monster_list.push(monsters[loc]);
        }

        monster_list.sort(function (m1, m2) {
            return monster_sort(m1.mon, m2.mon);
        });

        var new_monster_groups = [];
        var last_monster_group;
        for (var i = 0; i < monster_list.length; ++i)
        {
            var entry = monster_list[i];
            if (last_monster_group
                && can_combine(last_monster_group[0].mon, entry.mon))
            {
                last_monster_group.push(entry);
            }
            else
            {
                last_monster_group = [monster_list[i]];
                new_monster_groups.push(last_monster_group);
            }
        }

        return new_monster_groups;
    }

    // These need to be updated when the respective enums change
    var attitude_classes = [
        "hostile",
        "neutral",
        "strict_neutral",
        "good_neutral",
        "friendly"
    ];
    var mthreat_classes = [
        "trivial",
        "easy",
        "tough",
        "nasty"
    ];

    function update()
    {
        var grouped_monsters = group_monsters();

        var rows = Math.min(grouped_monsters.length, max_rows);
        var i = 0;
        for (; i < rows; ++i)
        {
            var monsters = grouped_monsters[i];
            var group, canvas, renderer;

            if (i >= monster_groups.length)
            {
                // Create a new row
                var node = $("<span class='group'>\
                                <canvas class='picture'></canvas>\
                                <span class='name'></span>\
                              </span>");
                $list.append(node);
                canvas = node.find("canvas")[0];
                renderer = new cr.DungeonCellRenderer();
                group = {
                    node: node,
                    canvas: canvas,
                    renderer: renderer,
                    name_span: node.find(".name"),
                    monsters: monsters,
                };
                monster_groups.push(group);
            }
            else
            {
                // Reuse the existing row
                group = monster_groups[i];
                canvas = group.canvas;
                renderer = group.renderer;
                group.monsters = monsters;
            }

            renderer.set_cell_size(dungeon_renderer.cell_width,
                                   dungeon_renderer.cell_height);
            for (key in dungeon_renderer)
            {
                if (key.match(/^glyph_mode/) || key == "display_mode"
                    || key == "smooth_scaling")
                    renderer[key] = dungeon_renderer[key];
            }
            var w = renderer.cell_width;
            var displayed_monsters = Math.min(monsters.length, 6);
            var needed_width = w * displayed_monsters;
            if ((canvas.width != needed_width)
                || (canvas.height != dungeon_renderer.cell_height))
            {
                canvas.width = needed_width;
                canvas.height = dungeon_renderer.cell_height;
                renderer.init(canvas);
            }

            for (var j = 0; j < displayed_monsters; ++j)
            {
                renderer.render_cell(monsters[j].loc.x, monsters[j].loc.y,
                                     j * w, 0);
            }

            if (monsters.length == 1)
                group.name_span.text(monsters[0].mon.name);
            else
                group.name_span.text(monsters.length + " " + monsters[0].mon.plural);

            $.each(attitude_classes, function (i, cls) {
                group.name_span.removeClass(cls);
            });
            $.each(mthreat_classes, function (i, cls) {
                group.name_span.removeClass(cls);
            });
            group.name_span.addClass(attitude_classes[monsters[0].mon.att]);
            group.name_span.addClass(mthreat_classes[monsters[0].mon.threat]);

            var ellipse = group.node.find(".ellipse");
            if ((i == rows - 1) && (rows < grouped_monsters.length))
            {
                if (ellipse.size() == 0)
                    group.node.append("<span class='ellipse'>...</span>");
            }
            else
                ellipse.remove();
        }

        for (; i < monster_groups.length; ++i)
        {
            monster_groups[i].node.remove();
        }
        monster_groups = monster_groups.slice(0, rows);
    }

    function set_max_rows(x)
    {
        max_rows = x;
    }

    function clear()
    {
        $list.empty();
        monster_groups = [];
        monsters = {};
    }

    return {
        update_loc: update_loc,
        update: update,
        set_max_rows: set_max_rows,
        clear: clear,
        get: function () { return monster_groups; }
    };
});
