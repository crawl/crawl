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
        "ac": "icy armour|protected from physical damage|sanguine armour|corpse armour|protection aura",
        "ev": "phasing|agile",
        "sh": "divine shield|corpse armour",
    }

    /**
     * Update the stats area bar of the given type.
     * @param name     The name of the bar.
     * @param propname The player property name, if it is different from the
     *                 name of the bar defined in game_data/template/game.html
     */
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
        // Use poison_survival to display our remaining hp after poison expires.
        if (name == "hp")
        {
            $("#stats_hp_bar_poison").css("width", 0);
            var poison_survival = player["poison_survival"]
            if (poison_survival < value)
            {
                var poison_bar = Math.round(10000 * (value - poison_survival)
                                            / max);
                full_bar = Math.round(10000 * poison_survival / max);
                $("#stats_hp_bar_poison").css("width", (poison_bar / 100)
                                              + "%");
            }
            if (full_bar + poison_bar + change_bar > 10000)
                change_bar = 10000 - poison_bar - full_bar;
        }
        else if (full_bar + change_bar > 10000)
        {
            change_bar = 10000 - full_bar;
        }

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

    function update_bar_noise()
    {
        player.noise_max = 1000;
        var level = player.adjusted_noise, max = player.noise_max;
        var old_value;
        if ("old_noise" in player)
            old_value = player.old_noise
        else
            old_value = level;
        if (level < 0)
            level = 0;
        if (old_value > max)
            old_value = max;

        // The adjusted_noise value has already been rescaled in
        // player.cc:get_adjusted_noise. It ranges from 0 to 1000.
        var adjusted_level = 0, noise_cat = "";

        // colors are set in `style.css` via the selector `data-level`.
        if (level <= 333)
            noise_cat = "quiet";
        else if (level <= 666)
            noise_cat = "loud";
        else if (level < 1000)
            noise_cat = "veryloud";
        else
            noise_cat = "superloud";

        var silenced = player.has_status("silence")
        if (silenced)
        {
            level = 0;
            old_value = 0;
            noise_cat = "blank";
            // I couldn't get this to work just directly putting the text in #stats_noise_bar,
            // because clearing the text later will adjust the span width.
            $("#stats_noise_status").text("Silenced");
        } else {
            $("#stats_noise_status").text("");
        }
        $("#stats_noise").attr("data-level", noise_cat);

        player.old_noise = level;


        var full_bar = Math.round(10000 * level / max);
        var change_bar = Math.round(10000 * Math.abs(old_value - level) / max);
        if (full_bar + change_bar > 10000)
        {
            change_bar = 10000 - full_bar;
        }

        if (player.wizard && !silenced)
        {
            // the exact value is too hard to interpret to show outside of
            // wizmode, because noise propagation is very complicated.
            $("#stats_noise").attr("data-shownum", true);
            $("#stats_noise_num").text(player.noise);
        }
        else
        {
            $("#stats_noise").attr("data-shownum", null);
            $("#stats_noise_num").text("");
        }

        $("#stats_noise_bar_full").css("width", (full_bar / 100) + "%");
        if (adjusted_level < old_value)
            $("#stats_noise_bar_decrease").css("width", (change_bar / 100) + "%");
        else
            $("#stats_noise_bar_decrease").css("width", 0);
    }

    function repeat_string(s, n)
    {
        return Array(n+1).join(s);
    }

    function index_to_letter(index)
    {
        if (index === -1)
            return "-"
        else if (index < 26)
            return String.fromCharCode("a".charCodeAt(0) + index);
        else
            return String.fromCharCode("A".charCodeAt(0) + index - 26);
    }

    function inventory_item_desc(index)
    {
        var item = player.inv[index];
        var elem = $("<span>");
        elem.text(item.name);
        if (item.col != -1 && item.col != null)
            elem.addClass("fg" + item.col);
        return elem;
    }

    function wielded_weapon()
    {
        var elem;
        var wielded = player.equip[enums.equip.WEAPON];
        if (wielded == -1)
        {
            elem = $("<span>");
            elem.text(player.unarmed_attack);
            elem.addClass("fg" + player.unarmed_attack_colour);
        }
        else
            elem = inventory_item_desc(wielded);

        if (player.has_status("corroded equipment"))
            elem.addClass("corroded_weapon");

        return elem;
    }

    function quiver()
    {
        if (!player.quiver_available)
        {
            var elem = $("<span>");
            elem.text("Quiver unavailable");
            elem.addClass("fg8");
            return elem;
        }
        else if (player.quiver_item == -1)
            return "Nothing quivered";
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
        else if (type == "ac" && player.has_status("corroded equipment"))
            elem.addClass("degenerated_defense");
        else if (type == "sh" && player.god == "Qazlal"
                 && player.piety_rank > 0)
            elem.addClass("boosted_defense");
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

    var simple_stats = ["hp", "hp_max", "mp", "mp_max", "xl", "progress"];
    /**
     * Update the stats pane area based on the player's current properties.
     */
    function update_stats_pane()
    {
        $("#stats_titleline").text(player.name + " " + player.title);
        $("#stats_wizmode").text(player.wizard ? "*WIZARD*" : "");

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
        }

        var species_god = player.species;
        if (player.god != "")
            species_god += " of " + player.god;
        if (player.god == "Xom")
        {
            if (player.piety_rank >=0)
            {
                $("#stats_piety").text(repeat_string(".",player.piety_rank) + "*"
                                       + repeat_string(".",5-player.piety_rank));
            }
            else
                $("#stats_piety").text("......"); // very special plaything
        }
        else if ((player.piety_rank > 0 || player.god != "")
                 && player.god != "Gozag")
        {
            $("#stats_piety").text(repeat_string("*", player.piety_rank)
                                   + repeat_string(".", 6-player.piety_rank));
        }
        else
            $("#stats_piety").text("");

        if (player.god == "Gozag")
        {
            $("#stats_gozag_gold_label").text(" Gold: ");
            $("#stats_gozag_gold_label").css("padding-left", "0.5em");
            $("#stats_gozag_gold").text(player.gold);
        } else {
            $("#stats_gozag_gold_label").text("");
            $("#stats_gozag_gold").text("");
            $("#stats_gozag_gold_label").css("padding-left", "0");
        }
        $("#stats_gozag_gold").toggleClass("boosted_stat", player.has_status("gold aura"));

        $("#stats_species_god").text(species_god);
        $("#stats_piety").toggleClass("penance", !!player.penance);
        $("#stats_piety").toggleClass("monk", player.god == "");

        for (var i = 0; i < simple_stats.length; ++i)
            $("#stats_" + simple_stats[i]).text(player[simple_stats[i]]);

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

        update_defense("ac");
        update_defense("ev");
        update_defense("sh");

        update_stat("str");
        update_stat("int");
        update_stat("dex");
        update_bar_noise();

        if (options.get("show_game_time") === true)
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

        $("#stats_weapon_letter").text(
            index_to_letter(player.equip[enums.equip.WEAPON]) + ")");
        $("#stats_weapon").html(wielded_weapon());
        $("#stats_quiver_letter").text(
            index_to_letter(player.quiver_item) + ")");
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

        var family = options.get("tile_font_stat_family");
        if (family !== "" && family !== "monospace")
        {
            family += ", monospace";
            $("#stats").css("font-family", family);
        }
    });

    comm.register_handlers({
        "player": handle_player_message,
    });

    $(document).off("game_init.player")
        .on("game_init.player", function () {
            $.extend(player, {
                name: "", god: "", title: "", species: "",
                hp: 0, hp_max: 0, real_hp_max: 0, poison_survival: 0,
                mp: 0, mp_max: 0,
                ac: 0, ev: 0, sh: 0,
                xl: 0, progress: 0,
                time: 0, time_delta: 0,
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
                noise: 0,
                adjusted_noise: 0
            });
            delete player["old_hp"];
            delete player["old_mp"];
            for (var i = 0; i < enums.equip.NUM_EQUIP; ++i)
                player.equip[i] = -1;
            last_time = null;
        });

    return player;
});
