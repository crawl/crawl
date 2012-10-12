define(["jquery", "comm", "./enums", "./map_knowledge", "./messages"],
function ($, comm, enums, map_knowledge, messages) {
    var player = {
        name: "", god: "",
        hp: 0, hp_max: 0,
        mp: 0, mp_max: 0,
        ac: 0, ev: 0, sh: 0,
        xl: 0, progress: 0,
        gold: 0,
        str: 0, int: 0, dex: 0,
        piety_rank: 0, penance: false,
        inv: {}, equip: {},
        pos: null
    };
    var last_time = null;
    window.player = player;

    for (var i = 0; i < enums.equip.NUM_EQUIP; ++i)
        player.equip[i] = -1;

    function update_bar(name)
    {
        var value = player[name], max = player[name + "_max"];
        var old_value;
        if ("old_" + name in player)
            old_value = player["old_" + name];
        else
            old_value = value;
        if (value < 0)
            value = 0;
        player["old_" + name] = value;
        var increase = old_value < value;
        var full_bar = (100 * (increase ? old_value : value) / max).toFixed(0);
        var change_bar = (100 * Math.abs(old_value - value) / max).toFixed(0);
        $("#stats_" + name + "_bar_full").css("width", full_bar + "%");
        $("#stats_" + name + "_bar_" + (increase ? "increase" : "decrease"))
            .css("width", change_bar + "%");
        $("#stats_" + name + "_bar_" + (increase ? "decrease" : "increase"))
            .css("width", 0);
    }

    function repeat_string(s, n)
    {
        return Array(n+1).join(s);
    }

    function index_to_letter(index)
    {
        if (index < 26)
            return String.fromCharCode("a".charCodeAt(0) + index);
        else
            return String.fromCharCode("A".charCodeAt(0) + index - 26);
    }

    function inventory_item_desc(index)
    {
        var item = player.inv[index];
        return index_to_letter(index) + ") " + item.name;
    }

    function wielded_weapon()
    {
        var wielded = player.equip[enums.equip.WEAPON];
        if (wielded == -1)
            return "-) " + player.unarmed_attack;
        else
            return inventory_item_desc(wielded);
    }

    function quiver()
    {
        if (player.quiver_item == -1)
            return "-) Nothing quivered";
        else
            return inventory_item_desc(player.quiver_item);
    }

    function stat_class(stat)
    {
        var val = player[stat];
        var max_val = player[stat + "_max"];
        if (val <= 0)
            return "zero_stat";

        // TODO: stat colour options

        // TODO: magically increased stats

        if (val < max_val)
            return "degenerated_stat";

        return "";
    }

    function update_stat(stat)
    {
        var val = player[stat];
        var max_val = player[stat + "_max"];
        var elem = $("<span>");
        elem.text(val);
        if (val < max_val)
        {
            elem.append(" (" + max_val + ")");
        }
        elem.addClass(stat_class(stat));
        $("#stats_" + stat).html(elem);
    }

    var simple_stats = ["hp", "hp_max", "mp", "mp_max",
                        "ac", "ev", "sh", "xl", "progress", "gold"];
    function update_stats_pane()
    {
        $("#stats_titleline").text(player.name + " the " + player.title);
        $("#stats_wizmode").text(player.wizard ? "*WIZARD*" : "");
        var species_god = player.species;
        if (player.god != "")
        {
            species_god += " of " + player.god;
            $("#stats_piety").text(repeat_string("*", player.piety_rank)
                                   + repeat_string(".", 6-player.piety_rank));
        }
        else
        {
            $("#stats_piety").text("");
        }
        $("#stats_species_god").text(species_god);
        $("#stats_piety").toggleClass("penance", !!player.penance);
        for (var i = 0; i < simple_stats.length; ++i)
        {
            $("#stats_" + simple_stats[i]).text(player[simple_stats[i]]);
        }
        if (player.real_hp_max != player.hp_max)
            $("#stats_real_hp_max").text("(" + player.real_hp_max + ")");
        else
            $("#stats_real_hp_max").text("");
        update_bar("hp");
        update_bar("mp");

        update_stat("str");
        update_stat("int");
        update_stat("dex");

        $("#stats_time").text((player.time / 10.0).toFixed(1));
        if (player.time_delta)
            $("#stats_time").append(" (" + (player.time_delta / 10.0).toFixed(1) + ")");
        var place_desc = player.place;
        if (player.depth) place_desc += ":" + player.depth;
        $("#stats_place").text(place_desc);

        var status = "";
        for (var status_light in player.status)
        {
            status += ("<span class='status_light fg"
                       + player.status[status_light].colour + "'>"
                       + status_light + "</span> ");
        }
        $("#stats_status_lights").html(status);

        $("#stats_weapon").html(wielded_weapon());
        $("#stats_quiver").html(quiver());
    }

    function handle_player_message(data)
    {
        for (var i in data.inv)
        {
            player.inv[i] = player.inv[i] || {};
            $.extend(player.inv[i], data.inv[i]);
        }
        $.extend(player.equip, data.equip);
        delete data.equip;
        delete data.inv;
        delete data.msg;

        $.extend(player, data);

        if ("time" in data)
        {
            if (last_time)
            {
                player.time_delta = player.time - last_time;
            }
            last_time = player.time;
            messages.new_command(true);
        }

        update_stats_pane();

        if (("hp" in data || "hp_max" in data ||
             "mp" in data || "mp_max" in data)
            && (player.pos != null))
        {
            map_knowledge.touch(player.pos);
        }
    }

    comm.register_handlers({
        "player": handle_player_message,
    });

    return player;
});
