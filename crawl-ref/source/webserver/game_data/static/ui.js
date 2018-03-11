define(["jquery", "comm", "client", "ui", "./enums", "./cell_renderer",
    "./util", "./scroller", "./tileinfo-main", "./tileinfo-gui"],
function ($, comm, client, ui, enums, cr, util, scroller, main, gui) {
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
        $popup.find(".body").html(fmt_body_txt(desc.body));
        scroller($popup.find(".body")[0]);
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

    function describe_spell(desc)
    {
        var $popup = $(".templates > .describe-spell").clone();
        $popup.find(".header > span").html(desc.title);
        $popup.find(".body").html(fmt_body_txt(desc.desc));
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
        var $popup = top_popup();
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
        var $popup = top_popup();
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
        $popup.find(".header > canvas").remove();
        $popup.find(".body").html(fmt_body_txt(desc.body));
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

    function update_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
        var $popup = top_popup();
        if (!$popup.hasClass("formatted-scroller"))
            return;
        var $scroller = $(scroller($popup.find(".body")).scrollElement);
        var line_height = 21;
        comm.send_message("formatted_scroller_scroll", {
            scroll: $scroller.scrollTop() / line_height,
        });
    }

    function formatted_scroller_onscroll()
    {
        if (!update_server_scroll_timeout)
            update_server_scroll_timeout = setTimeout(update_server_scroll, 100);
    }

    function formatted_scroller(desc)
    {
        var $popup = $(".templates > .formatted-scroller").clone();
        var $body = $popup.children(".body");
        var $more = $popup.children(".more");
        $body.html(util.formatted_string_to_html(desc.text));
        $more.html(util.formatted_string_to_html(desc.more));
        scroller($body[0]).scrollElement
            .addEventListener("scroll", formatted_scroller_onscroll);
        return $popup;
    }

    function formatted_scroller_update(msg)
    {
        var $popup = top_popup();
        if (!$popup.hasClass("formatted-scroller"))
            return;
        if (msg.text !== undefined)
        {
            var $body = $(scroller($popup.find(".body")[0]).contentElement);
            $body.html(util.formatted_string_to_html(msg.text));
        }
        if (msg.scroll !== undefined && (!msg.from_webtiles || client.is_watching()))
        {
            var $scroller = $(scroller($popup.find(".body")[0]).scrollElement);
            var line_height = 21;
            $scroller.scrollTop(msg.scroll*line_height);
        }
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
        };
        var handler = ui_handlers[msg.type];
        if (handler)
            handler(msg);
    }

    comm.register_handlers({
        "ui-push": recv_ui_push,
        "ui-pop": recv_ui_pop,
        "ui-stack": recv_ui_stack,
        "ui-state": recv_ui_state,
    });

    function top_popup()
    {
        var $popup = $("#ui-stack").children().last();
        if ($popup.length === 0)
            return;
        return $popup.find(".ui-popup-inner").children().eq(0);
    }

    function ui_key_handler (ev)
    {
        if (client.is_watching())
            return;
        var $popup = top_popup();
        if ($popup === undefined)
            return;

        var new_ev = $.extend({}, ev);
        new_ev.type = ev.type.replace(/^game_/, "");
        $popup.triggerHandler(new_ev);

        if (new_ev.default_prevented)
            ev.preventDefault();
        if (new_ev.propagation_stopped)
            ev.stopImmediatePropagation();
    }

    function ui_cleanup()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
    }

    $(document).off("game_init.ui")
        .on("game_init.ui", function () {
        $(document).off("game_keydown.ui game_keypress.ui")
            .on("game_keydown.ui", ui_key_handler)
            .on("game_keypress.ui", ui_key_handler);
        $(document).off("game_cleanup.ui")
                   .on("game_cleanup.ui", ui_cleanup);
    });

    return {
        register_handlers: register_ui_handlers,
    };
});
