define(["jquery", "comm", "./enums", "./map_knowledge", "./messages",
        "./options"],
function ($, comm, enums, map_knowledge, messages, options) {
    "use strict";

    var player = {}, last_time;

    var stat_boosters = {
        "str": "vitalised|mighty|berserk",
        "int": "vitalised|brilliant",
        "dex": "vitalised|agile",
        "hp": "divinely vigorous|berserk",
        "mp": "divinely vigorous"
    };

    var defense_boosters = {
        "ac": "icy armour|stone skin",
        "ev": "phasing|agile",
        "sh": "shielded",
    }

    function update_bar(name, propname)
    {
        if (!propname) propname = name;

        var value = player[propname], max = player[propname + "_max"];
        var old_value;
        if ("old_" + propname in player)
            old_value = player["old_" + propname];
        else
            old_value = value;
        if (value < 0)
            value = 0;
        if (old_value > max)
            old_value = max;
        player["old_" + propname] = value;
        var increase = old_value < value;
        var full_bar = Math.round(10000 * (increase ? old_value : value) / max);
        var change_bar = Math.round(10000 * Math.abs(old_value - value) / max);
        if (full_bar + change_bar > 10000)
            change_bar = 10000 - full_bar;
        $("#stats_" + name + "_bar_full").css("width", (full_bar / 100) + "%");
        $("#stats_" + name + "_bar_" + (increase ? "increase" : "decrease"))
            .css("width", (change_bar / 100) + "%");
        $("#stats_" + name + "_bar_" + (increase ? "decrease" : "increase"))
            .css("width", 0);
    }

    function update_bar_contam()
    {
        player.contam_max = 16000;
        update_bar("contam");

        // Calculate level for the colour
        var contam_level = 0;

        if (player.contam > 25000)
            contam_level = 4;
        else if (player.contam > 15000)
            contam_level = 3;
        else if (player.contam > 5000)
            contam_level = 2;
        else if (player.contam > 0)
            contam_level = 1;

        $("#stats_contamline").attr("data-contam", contam_level);
    }

    function update_bar_heat()
    {
        player.heat_max = 15; // Value of TEMP_MAX
        update_bar("heat");
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
        var elem = $("<span>");
        elem.text(index_to_letter(index) + ") " + item.name);
        if (item.col != -1)
            elem.addClass("fg" + item.col);
        return elem;
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

    player.has_status_light = function (status_light, col)
    {
        for (var i = 0; i < player.status.length; ++i)
        {
            if (player.status[i].light &&
                player.status[i].light.match(status_light) &&
                (col == null || player.status[i].col == col))
                return true;
        }
        return false;
    }
    player.has_status = function (status_name, col)
    {
        for (var i = 0; i < player.status.length; ++i)
        {
            if (player.status[i].text &&
                player.status[i].text.match(status_name) &&
                (col == null || player.status[i].col == col))
                return true;
        }
        return false;
    }
    player.incapacitated = function incapacitated()
    {
        // FIXME: does this cover all ATTR_HELD cases?
        return player.has_status("paralysed|petrified|sleeping")
               || player.has_status("confused|petrifying")
               || player.has_status("held", 4);
    }

    function update_defense(type)
    {
        var elem = $("#stats_"+type);
        elem.text(player[type]);
        elem.removeClass();
        if (type == "sh" && player.incapacitated()
            && player.equip[enums.equip.SHIELD] != -1)
            elem.addClass("degenerated_defense");
        else if (player.has_status(defense_boosters[type]))
            elem.addClass("boosted_defense");
        else if (type == "ac" && player.has_status("icemail depleted"))
            elem.addClass("degenerated_defense");
    }

    function stat_class(stat)
    {
        var val = player[stat];
        var max_val = player[stat + "_max"];
        if (player.has_status("lost " + stat))
            return "zero_stat";

        var limits = options.get("stat_colour")
        for (var i in limits)
            if (val <= limits[i].value)
                return "colour_" + limits[i].colour;

        if (player.has_status(stat_boosters[stat]))
            return "boosted_stat";

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

    function percentage_color(name)
    {
        var real = false;
        if (player["real_" + name + "_max"] != player[name + "_max"])
            real = true;

        $("#stats_" + name).removeClass();
        $("#stats_" + name + "_separator").removeClass();
        $("#stats_" + name + "_max").removeClass();
        if (real)
            $("#stats_real_" + name + "_max").removeClass();

        if (player.has_status(stat_boosters[name]))
        {
            $("#stats_" + name).addClass("boosted_stat");
            $("#stats_" + name + "_separator").addClass("boosted_stat");
            $("#stats_" + name + "_max").addClass("boosted_stat");
            if (real)
                $("#stats_real_" + name + "_max").addClass("boosted_stat");
            return;
        }

        var val = player[name] / player[(real ? "real_" : "") + name + "_max"]
                  * 100;
        var limits = options.get(name + "_colour");
        var colour = null;
        for (var i in limits)
            if (val <= limits[i].value)
                colour = limits[i].colour;

        if (colour)
            $("#stats_" + name).addClass("colour_" + colour);
    }

    var simple_stats = ["hp", "hp_max", "mp", "mp_max", "xl", "progress", "gold"];
    function update_stats_pane()
    {
        $("#stats_titleline").text(player.name + " the " + player.title);
        $("#stats_wizmode").text(player.wizard ? "*WIZARD*" : "");

        var do_temperature = false;
        var do_contam = false;

        // Setup species
        // TODO: Move to a proper initialisation task
        if ($("#stats").attr("data-species") != player.species)
        {
            $("#stats").attr("data-species", player.species);
            var hp_cap;
            if (player.real_hp_max != player.hp_max)
            {
                hp_cap = "HP";
                if (player.species == "Djinni")
                    hp_cap = "EP";
            }
            else
            {
                hp_cap = "Health";
                if (player.species == "Djinni")
                    hp_cap = "Essence";
            }
            $("#stats_hpline > .stats_caption").text(hp_cap+":");
        }
        switch (player.species)
        {
            case "Djinni":
                do_contam = true;
                break;
            case "Lava Orc":
                do_temperature = true;
                break;
        }

        var species_god = player.species;
        if (player.god != "")
            species_god += " of " + player.god;
        if (player.god == "Xom")
        {
            if (player.piety_rank == 1)
                $("#stats_piety").text("- getting BORED");
            else if (player.piety_rank == 2)
                $("#stats_piety").text("- BORED");
            else
                $("#stats_piety").text("");
        }
        else if (player.piety_rank > 0 || player.god != "")
        {
            $("#stats_piety").text(repeat_string("*", player.piety_rank)
                                   + repeat_string(".", 6-player.piety_rank));
        }
        else
            $("#stats_piety").text("");
        $("#stats_species_god").text(species_god);
        $("#stats_piety").toggleClass("penance", !!player.penance);
        $("#stats_piety").toggleClass("monk", player.god == "");

        for (var i = 0; i < simple_stats.length; ++i)
        {
            $("#stats_" + simple_stats[i]).text(player[simple_stats[i]]);
        }

        if (player.zp !== null)
        {
            $(".stats_zot_defense").show();
            $("#stats_zp").text(player.zp);
        }
        else
            $(".stats_zot_defense").hide();

        if (player.real_hp_max != player.hp_max)
            $("#stats_real_hp_max").text("(" + player.real_hp_max + ")");
        else
            $("#stats_real_hp_max").text("");

        percentage_color("hp");
        percentage_color("mp");
        update_bar("hp");
        if (do_contam)
            update_bar_contam();
        else
            update_bar("mp");
        if (do_temperature)
            update_bar_heat();

        update_defense("ac");
        update_defense("ev");
        update_defense("sh");

        update_stat("str");
        update_stat("int");
        update_stat("dex");

        if (options.get("show_game_turns") === true)
        {
            $("#stats_time_caption").text("Time:");
            $("#stats_time").text((player.time / 10.0).toFixed(1));
            if (player.time_delta)
                $("#stats_time").append(" (" + (player.time_delta / 10.0).toFixed(1) + ")");
        }
        else
        {
            $("#stats_time_caption").text("Turn:");
            $("#stats_time").text(player.turn);
        }

        var place_desc = player.place;
        if (player.depth) place_desc += ":" + player.depth;
        $("#stats_place").text(place_desc);

        var status = "";
        for (var i = 0; i < player.status.length; ++i)
        {
            var status_inf = player.status[i];
            if (!status_inf.light) continue;
            status += ("<span class='status_light fg"
                       + status_inf.col + "'>"
                       + status_inf.light + "</span> ");
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

        if ("hp" in data || "hp_max" in data ||
            "mp" in data || "mp_max" in data)
        {
            map_knowledge.touch(player.pos);
            $("#dungeon").trigger("update_cells", [[player.pos]]);
        }
    }

    options.add_listener(function ()
    {
        if (player.name !== "")
            update_stats_pane();

        if (options.get("tile_font_stat_size") === 0)
            $("#stats").css("font-size", "");
        else
        {
            $("#stats").css("font-size",
                options.get("tile_font_stat_size") + "px");
        }
    });

    comm.register_handlers({
        "player": handle_player_message,
    });

    $(document).off("game_init.player")
        .on("game_init.player", function () {
            $.extend(player, {
                name: "", god: "", title: "", species: "",
                hp: 0, hp_max: 0, real_hp_max: 0,
                mp: 0, mp_max: 0,
                ac: 0, ev: 0, sh: 0,
                xl: 0, progress: 0,
                zp: null,
                gold: 0,
                str: 0, int: 0, dex: 0,
                str_max: 0, int_max: 0, dex_max: 0,
                piety_rank: 0, penance: false,
                status: [],
                inv: {}, equip: {}, quiver_item: -1,
                unarmed_attack: "",
                pos: {x: 0, y: 0},
                wizard: 0,
                depth: 0, place: "",
                contam: 0,
                heat: 0
            });
            delete player["old_hp"];
            delete player["old_mp"];
            for (var i = 0; i < enums.equip.NUM_EQUIP; ++i)
                player.equip[i] = -1;
            last_time = null;
        });

    return player;
});
