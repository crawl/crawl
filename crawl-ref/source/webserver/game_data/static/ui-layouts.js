define(["jquery", "comm", "client", "./ui", "./enums", "./cell_renderer",
    "./util", "./scroller", "./tileinfo-main", "./tileinfo-gui", "./tileinfo-player"],
function ($, comm, client, ui, enums, cr, util, scroller, main, gui, player) {
    "use strict";

    function fmt_body_txt(txt)
    {
        return txt
            .split("\n\n")
            .map(function (s) { return "<p>"+s.trim()+"</p>"; })
            .filter(function (s) { return s !== "<p></p>"; })
            .join("")
            .split("\n").join("<br>");
    }

    function _fmt_spellset_html(desc)
    {
        var desc_parts = desc.split("SPELLSET_PLACEHOLDER")
            .map(fmt_body_txt)
        if (desc_parts.length == 2)
            desc_parts.splice(1, 0, "<div id=spellset_placeholder></div>")
        return desc_parts.join("");
    }

    function _fmt_spells_list(root, spellset)
    {
        var $container = root.find("#spellset_placeholder");
        $container.attr("id", "").addClass("menu_contents spellset");
        if (spellset.length === 0)
        {
            $container.remove();
            return;
        }
        $.each(spellset, function (i, book) {
            $container.append(book.label);
            var $list = $("<ol>");
            $.each(book.spells, function (i, spell) {
                var $item = $("<li class=selectable>"), $canvas = $("<canvas>");
                var letter = spell.letter;

                var renderer = new cr.DungeonCellRenderer();
                util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
                renderer.init($canvas[0]);
                renderer.draw_from_texture(spell.tile, 0, 0, enums.texture.GUI, 0, 0, 0, false);
                $item.append($canvas);

                var label = " " + letter + " - "+spell.title;
                $item.append("<span>" + label + "</span>");

                if (spell.hex_chance !== undefined)
                    $item.append("<span>("+spell.hex_chance+"%) </span>");

                $list.append($item);
                $item.addClass("fg"+spell.colour);
                $item.on("click", function () {
                    comm.send_message("input", { text: letter });
                });
            });
            $container.append($list);
        });
    }

    function paneset_cycle($el, next)
    {
        var $panes = $el.children(".pane");
        var $current = $panes.filter(".current").removeClass("current");
        if (next === undefined)
            next = $panes.index($current) + 1;
        $panes.eq(next % $panes.length).addClass("current");
    }

    function describe_generic(desc)
    {
        var $popup = $(".templates > .describe-generic").clone();
        var $body = $popup.children(".body");
        if (desc.title === "")
            $popup.children(".header").remove();
        else
            $popup.find(".header > span").html(desc.title);
        $body.html(fmt_body_txt(desc.body + desc.footer));
        scroller($popup.children(".body")[0])
            .contentElement.className += " describe-generic-body";

        var canvas = $popup.find(".header > canvas");
        if (desc.tile)
        {
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0, desc.tile.ymax, false);
        }
        else
            canvas.remove();

        return $popup;
    }

    function describe_feature_wide(desc)
    {
        var $popup = $(".templates > .describe-feature").clone();
        var $feat_tmpl = $(".templates > .describe-generic");
        desc.feats.forEach(function (feat) {
            var $feat = $feat_tmpl.clone().removeClass("hidden").addClass("describe-feature-feat");
            $feat.find(".header > span").html(feat.title);
            if (feat.body != feat.title)
                $feat.find(".body").html(feat.body);
            else
                $feat.find(".body").remove();

            var canvas = $feat.find(".header > canvas");
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(feat.tile.t, 0, 0, feat.tile.tex, 0, 0, feat.tile.ymax, false);
            $popup.append($feat);
        });
        scroller($popup[0]);
        return $popup;
    }

    function describe_item(desc)
    {
        var $popup = $(".templates > .describe-item").clone();
        $popup.find(".header > span").html(desc.title);
        var $body = $popup.find(".body");
        $body.html(_fmt_spellset_html(desc.body));
        _fmt_spells_list($body, desc.spellset);
        scroller($body[0]);
        if (desc.actions !== "")
            $popup.find(".actions").html(desc.actions);
        else
            $popup.find(".actions").remove();

        var canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init(canvas[0]);

        desc.tiles.forEach(function (tile) {
            renderer.draw_from_texture(tile.t, 0, 0, tile.tex, 0, 0, tile.ymax, false);
        });

        return $popup;
    }

    function format_spell_html(desc)
    {
        var parts = desc.match(/(.*\n\n)(Level: [^\n]+)(\n\n.*)/);
        if (parts == null || parts.length != 4)
            return fmt_body_txt(desc);
        parts[2] = parts[2].replace(/ /g, '&nbsp;');
        return fmt_body_txt(parts[1]+parts[2]+parts[3])
    }

    function describe_spell(desc)
    {
        var $popup = $(".templates > .describe-spell").clone();
        $popup.find(".header > span").html(desc.title);
        $popup.find(".body").html(format_spell_html(desc.desc));
        scroller($popup.find(".body")[0]);
        if (desc.can_mem)
            $popup.find(".actions").removeClass("hidden");

        var canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init(canvas[0]);
        renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0, desc.tile.ymax, false);

        return $popup;
    }

    function describe_cards(desc)
    {
        var $popup = $(".templates > .describe-cards").clone();
        var $card_tmpl = $(".templates > .describe-generic");
        var t = main.MISC_CARD, tex = enums.texture.DEFAULT;
        desc.cards.forEach(function (card) {
            var $card = $card_tmpl.clone().removeClass("hidden").addClass("describe-card");
            $card.find(".header > span").html(card.name);
            $card.find(".body").html(fmt_body_txt(card.desc));

            var canvas = $card.find(".header > canvas");
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(t, 0, 0, tex, 0, 0, 0, false);
            $popup.append($card);
        });
        scroller($popup[0]);
        return $popup;
    }

    function mutations(desc)
    {
        var $popup = $(".templates > .mutations").clone();
        var $body = $popup.find(".body");
        var $footer = $popup.find(".footer");
        var $muts = $body.children().first(), $vamp = $body.children().last();
        $muts.html(fmt_body_txt(util.formatted_string_to_html(desc.mutations)));
        scroller($muts[0]);
        paneset_cycle($body);

        if (desc.vampire !== undefined)
        {
            var css = ".vamp-attrs th:nth-child(%d) { background: #111; }";
            css += " .vamp-attrs td:nth-child(%d) { color: white; background: #111; }";
            css = css.replace(/%d/g, desc.vampire + 1);
            $vamp.children("style").html(css);
            scroller($vamp[0]);
            paneset_cycle($footer);
        }
        else
        {
            $vamp.remove();
            $footer.remove();
        }

        $popup.on("keydown", function (event) {
            if (event.key === "!" || event.key === "^")
            {
                paneset_cycle($body);
                paneset_cycle($footer);
            }
        });
        return $popup;
    }

    function mutations_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("mutations"))
            return;
        if (msg.pane !== undefined && client.is_watching())
        {
            paneset_cycle($popup.children(".body"), msg.pane);
            paneset_cycle($popup.children(".footer"), msg.pane);
        }
    }

    function describe_god(desc)
    {
        var $popup = $(".templates > .describe-god").clone();
        $popup.find(".header > span").addClass("fg"+desc.colour).html(desc.name);

        var canvas = $popup.find(".header > canvas")[0];
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas, renderer.cell_width, renderer.cell_height);
        renderer.init(canvas);
        renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0, 0, false);

        var $body = $popup.children(".body");
        var $footer = $popup.find(".footer > .paneset");
        var $panes = $body.children(".pane");

        if (!desc.is_altar)
            $footer.find('.join-keyhelp').remove();

        $panes.eq(0).find(".desc").html(desc.description);
        $panes.eq(0).find(".god-favour td.title").addClass("fg"+desc.colour).html(desc.title);
        $panes.eq(0).find(".god-favour td.favour").addClass("fg"+desc.colour).html(desc.favour);
        var powers_list = desc.powers_list.split("\n").slice(3, -1);
        var $powers = $panes.eq(0).find(".god-powers");
        var re = /^(.*\.) *( \(.*\))?$/;
        for (var i = 0; i < powers_list.length; i++)
        {
            var matches = powers_list[i].match(re);
            var power = matches[1], cost = matches[2] || "";
            $powers.append("<div class=power><div>"+power+"</div><div>"+cost+"</div></div>");
        }

        $panes.eq(1).html(fmt_body_txt(util.formatted_string_to_html(desc.powers)));
        $panes.eq(2).html(fmt_body_txt(util.formatted_string_to_html(desc.wrath)));
        for (var i = 0; i < 3; i++)
            scroller($panes.eq(i)[0]);

        $popup.on("keydown", function (event) {
            if (event.key === "!" || event.key === "^")
            {
                paneset_cycle($body);
                paneset_cycle($footer);
            }
        });
        paneset_cycle($body);
        paneset_cycle($footer);

        return $popup;
    }

    function describe_god_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("describe-god"))
            return;
        if (msg.pane !== undefined && client.is_watching())
        {
            paneset_cycle($popup.children(".body"), msg.pane);
            var $footer = $popup.find(".footer > .paneset");
            if ($footer.length > 0)
                paneset_cycle($footer, msg.pane);
        }
        if (msg.prompt !== undefined)
            $popup.children(".footer").html(msg.prompt).addClass("fg3");
    }

    function describe_monster(desc)
    {
        var $popup = $(".templates > .describe-generic").clone();
        $popup.find(".header > span").html(desc.title);
        $popup.find(".body").html(_fmt_spellset_html(desc.body));
        _fmt_spells_list($popup.find(".body"), desc.spellset);

        var $canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init($canvas[0]);

        if ((desc.fg_idx >= main.MAIN_MAX) && desc.doll)
        {
            var mcache_map = {};
            if (desc.mcache)
            {
                for (var i = 0; i < desc.mcache.length; ++i)
                    mcache_map[desc.mcache[i][0]] = i;
            }
            $.each(desc.doll, function (i, doll_part) {
                var xofs = 0;
                var yofs = Math.max(0, player.get_tile_info(desc.doll[0][0]).h - 32);
                if (mcache_map[doll_part[0]] !== undefined)
                {
                    var mind = mcache_map[doll_part[0]];
                    xofs = desc.mcache[mind][1];
                    yofs += desc.mcache[mind][2];
                }
                renderer.draw_player(doll_part[0],
                        0, 0, xofs, yofs, doll_part[1]);
            });
        }

        if ((desc.fg_idx >= player.MCACHE_START) && desc.mcache)
        {
            $.each(desc.mcache, function (i, mcache_part) {
                if (mcache_part) {
                    var yofs = Math.max(0, player.get_tile_info(mcache_part[0]).h - 32);
                    renderer.draw_player(mcache_part[0],
                            0, 0, mcache_part[1], mcache_part[2]+yofs);
                }
            });
        }

        renderer.draw_foreground(0, 0, { t: {
            fg: enums.prepare_fg_flags(desc.flag),
            bg: 0,
        }});

        scroller($popup.find(".body")[0]);
        return $popup;
    }

    function version(desc)
    {
        var $popup = $(".templates > .describe-generic").clone();
        $popup.find(".header > span").html(desc.information);
        var $body = $popup.find(".body");
        $body.html(fmt_body_txt(desc.features) + fmt_body_txt(desc.changes));
        scroller($body[0]);

        var t = gui.STARTUP_STONESOUP, tex = enums.texture.GUI;
        var $canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init($canvas[0]);
        renderer.draw_from_texture(t, 0, 0, tex, 0, 0, 0, false);

        return $popup;
    }

    var update_server_scroll_timeout = null;
    var formatted_scroller_monitor_scrolling = true;

    function formatted_scroller_line_height(body)
    {
        var $body = $(scroller(body).contentElement);
        var span = $body.children("span")[0];
        if (span == null) {
            $body.html("<span style='visibility:hidden;position:absolute;'> </span>"+$body.html());
            span = $body.children("span")[0];
        }
        var rect = span.getBoundingClientRect();
        return rect.bottom - rect.top;
    }

    function update_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
        var $popup = ui.top_popup();
        if (!$popup.hasClass("formatted-scroller"))
            return;
        var body = $popup.find(".body")[0];
        var container = scroller(body).scrollElement;
        var line_height = formatted_scroller_line_height(body);
        comm.send_message("formatted_scroller_scroll", {
            scroll: Math.round(container.scrollTop / line_height),
        });
    }

    function formatted_scroller_onscroll()
    {
        if (!formatted_scroller_monitor_scrolling)
        {
            // don't tell the server we scrolled, when we did so because the
            // server told us to; not absolutely necessary, but nice to have
            formatted_scroller_monitor_scrolling = true;
            return;
        }
        if (!update_server_scroll_timeout)
            update_server_scroll_timeout = setTimeout(update_server_scroll, 100);
    }

    function formatted_scroller(desc)
    {
        var $popup = $(".templates > .formatted-scroller").clone();
        var $body = $popup.children(".body");
        var $more = $popup.children(".more");
        var body_html = util.formatted_string_to_html(desc.text);
        if (desc.highlight !== "")
        {
            var rexp = '[^\n]*'+desc.highlight+'[^\n]*\n?';
            var re = new RegExp(rexp, 'g');
            console.log(rexp);
            body_html = body_html.replace(re, function (line) {
                console.log(line)
                return "<span class=highlight>" + line + "</span>";
            });
        }
        $body.html(body_html);
        $more.html(util.formatted_string_to_html(desc.more));
        var scroll_elem = scroller($body[0]).scrollElement;
        scroll_elem.addEventListener("scroll", formatted_scroller_onscroll);
        return $popup;
    }

    function formatted_scroller_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("formatted-scroller"))
            return;
        if (msg.text !== undefined)
        {
            var $body = $(scroller($popup.find(".body")[0]).contentElement);
            $body.html(util.formatted_string_to_html(msg.text));
        }
        if (msg.scroll !== undefined && (!msg.from_webtiles || client.is_watching()))
        {
            var body = $popup.find(".body")[0];
            var $scroller = $(scroller(body).scrollElement);
            if (msg.scroll == 2147483647) // FS_START_AT_END
            {
                // special case for webkit: excessively large values don't work
                var inner_h = $(scroller(body).contentElement).outerHeight();
                $scroller[0].scrollTop = inner_h - $(body).outerHeight();
            }
            else
            {
                var line_height = formatted_scroller_line_height(body);
                msg.scroll *= line_height;
                formatted_scroller_monitor_scrolling = false;
                $scroller[0].scrollTop = msg.scroll;
            }
        }
    }

    function msgwin_get_line(msg)
    {
        var $popup = $(".templates > .msgwin-get-line").clone();
        $popup.children(".header").html(util.formatted_string_to_html(msg.prompt));
        $popup.children(".body")[0].textContent = msg.text;
        return $popup;
    }
    function msgwin_get_line_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("msgwin-get-line"))
            return;
        $popup.children(".body")[0].textContent = msg.text;
    }

    var ui_handlers = {
        "describe-generic" : describe_generic,
        "describe-feature-wide" : describe_feature_wide,
        "describe-item" : describe_item,
        "describe-spell" : describe_spell,
        "describe-cards" : describe_cards,
        "mutations" : mutations,
        "describe-god" : describe_god,
        "describe-monster" : describe_monster,
        "version" : version,
        "formatted-scroller" : formatted_scroller,
        "msgwin-get-line" : msgwin_get_line,
    };

    function register_ui_handlers(dict)
    {
        $.extend(ui_handlers, dict);
    }

    function recv_ui_push(msg)
    {
        var handler = ui_handlers[msg.type];
        var popup = handler ? handler(msg) : $("<div>Unhandled UI type "+msg.type+"</div>");
        ui.show_popup(popup);
    }

    function recv_ui_pop(msg)
    {
        ui.hide_popup();
    }

    function recv_ui_stack(msg)
    {
        if (!client.is_watching())
            return;
        for (var i = 0; i < msg.items.length; i++)
            comm.handle_message(msg.items[i]);
    }

    function recv_ui_state(msg)
    {
        var ui_handlers = {
            "mutations" : mutations_update,
            "describe-god" : describe_god_update,
            "formatted-scroller" : formatted_scroller_update,
            "msgwin-get-line" : msgwin_get_line_update,
        };
        var handler = ui_handlers[msg.type];
        if (handler)
            handler(msg);
    }

    function ui_layouts_cleanup()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
    }

    $(document).off("game_init.ui-layouts")
        .on("game_init.ui-layouts", function () {
        $(document).off("game_cleanup.ui-layouts")
                   .on("game_cleanup.ui-layouts", ui_layouts_cleanup);
    });

    comm.register_handlers({
        "ui-push": recv_ui_push,
        "ui-pop": recv_ui_pop,
        "ui-stack": recv_ui_stack,
        "ui-state": recv_ui_state,
    });

    return {
        register_handlers: register_ui_handlers,
    };
});
