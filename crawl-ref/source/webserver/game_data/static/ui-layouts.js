define(["jquery", "comm", "client", "./ui", "./enums", "./cell_renderer",
    "./util", "./scroller", "./tileinfo-main", "./tileinfo-gui",
    "./tileinfo-player", "./options"],
function ($, comm, client, ui, enums, cr, util, scroller, main, gui, player, options) {
    "use strict";

    var describe_scale = 2.0;

    function fmt_body_txt(txt)
    {
        return txt
            // convert double linebreaks into paragraph markers
            .split("\n\n")
            .map(function (s) {
                return "<pre>" + util.formatted_string_to_html(s) + "</pre>";
            })
            .filter(function (s) { return s !== "<pre></pre>"; })
            .join("")
            // replace single linebreaks with manual linebreaks
            .split("\n")
            .join("<br>");
    }

    function _fmt_spellset_html(desc)
    {
        var desc_parts = desc.split("SPELLSET_PLACEHOLDER")
            .map(fmt_body_txt)
        if (desc_parts.length == 2)
            desc_parts.splice(1, 0, "<div id=spellset_placeholder></div>")
        return desc_parts.join("");
    }

    function _fmt_spells_list(root, spellset, colour)
    {
        var $container = root.find("#spellset_placeholder");
        // XX this container only seems to be added if there are spells, do
        // we actually need to remove it again?
        if ($container.length === 0 && spellset.length !== 0)
        {
            root.prepend("<div class='fg4'>Buggy spellset!</div>");
            return;
        }
        $container.attr("id", "").addClass("menu_contents spellset");
        if (spellset.length === 0)
        {
            $container.remove();
            return;
        }
        $.each(spellset, function (i, book) {
            if (book.label.trim())
                $container.append(book.label);
            var $list = $("<ol>");
            $.each(book.spells, function (i, spell) {
                var $item = $("<li class=selectable>");
                var $canvas = $("<canvas class='glyph-mode-hidden'>");
                var letter = spell.letter;

                var renderer = new cr.DungeonCellRenderer();
                util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
                renderer.init($canvas[0]);
                renderer.draw_from_texture(spell.tile, 0, 0, enums.texture.GUI, 0, 0, 0, false);
                $item.append($canvas);

                var label = " " + letter + " - " + spell.title;
                $item.append("<span>" + util.formatted_string_to_html(label) + "</span>");

                if (spell.effect !== undefined)
                    $item.append("<span>" + util.formatted_string_to_html(spell.effect) + " </span>");
                if (spell.range_string !== undefined)
                    $item.append("<span>" + util.formatted_string_to_html(spell.range_string) + " </span>");

                $list.append($item);
                if (colour)
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

    function progress_bar(msg)
    {
        var $popup = $(".templates > .progress-bar").clone();
        if (msg.title === "")
            $popup.children(".header").remove();
        else
            $popup.find(".header > span").html(msg.title);
        $popup.children(".bar-text").html("<span>" +
            util.formatted_string_to_html(msg.bar_text) + "</span>");
        $popup.children(".status > span").html(msg.status);
        return $popup;
    }

    function progress_bar_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("progress-bar"))
            return;
        $popup.children(".bar-text").html("<span>" +
                    util.formatted_string_to_html(msg.bar_text) + "</span>");
        $popup.children(".status")[0].textContent = msg.status;
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
        var s = scroller($popup.children(".body")[0]);
        s.contentElement.className += " describe-generic-body";
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });

        var canvas = $popup.find(".header > canvas");
        if (desc.tile)
        {
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0],
                renderer.cell_width * describe_scale,
                renderer.cell_height * describe_scale);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0,
                desc.tile.ymax, false, describe_scale);
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
            {
                var text = feat.body;
                if (feat.quote)
                     text += "\n\n" + feat.quote;
                $feat.find(".body").html(util.formatted_string_to_html(text));
            }
            else
                $feat.find(".body").remove();

            var canvas = $feat.find(".header > canvas");
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0],
                renderer.cell_width * describe_scale,
                renderer.cell_height * describe_scale);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(feat.tile.t, 0, 0, feat.tile.tex, 0, 0,
                feat.tile.ymax, false, describe_scale);
            $popup.append($feat);
        });
        if (desc.actions)
        {
            $popup.append("<div class=actions></div>");
            $popup.find(".actions").html(clickify_actions(desc.actions));
        }

        var s = scroller($popup[0]);
        $popup.on("keydown keypress", function (event) {
            var key = String.fromCharCode(event.which);
            if (key != "<" && key != ">") // XX not always
                scroller_handle_key(s, event);
        });
        return $popup;
    }

    // Given some string like "e(v)oke", produce a span with the `data-hotkey`
    // attribute set on that span. This auto-enables clicking. This will
    // handle both a final period, and a sequence of prefix words that do not
    // have a hotkey marked in them.
    // TODO: it might be more robust to produce structured info on the server
    // side...
    function clickify_action(action_text)
    {
        var suffix = "";
        var prefix = "";
        // makes some assumptions about how this is joined...see describe.cc
        // _actions_desc.
        if (action_text.endsWith(".")) // could be more elegant...
        {
            suffix = ".";
            action_text = action_text.slice(0, -1);
        }
        if (action_text.startsWith("or "))
        {
            prefix = "or ";
            action_text = action_text.substr(3);
        }
        var hotkeys = action_text.match(/\(.\)/); // very inclusive for the key name
        var data_attr = ""
        if (hotkeys)
            data_attr = " data-hotkey='" + hotkeys[0][1] + "'";
        return prefix
            + "<span" + data_attr + ">" + action_text + "</span>"
            + suffix;
    }

    // Turn a list of actions like that found in the describe item popup into
    // clickable links.
    function clickify_actions(actions_text)
    {
        // assumes that the list is joined via ", ", including the final
        // element. (I.e. this will break without the Oxford comma.)
        var words = actions_text.split(", ");
        var linkized = [];
        words.forEach(function(w) {
            linkized.push(clickify_action(w));
        });
        return linkized.join(", ");
    }

    function describe_item(desc)
    {
        var $popup = $(".templates > .describe-item").clone();
        $popup.find(".header > span").html(desc.title);
        var $body = $popup.find(".body");
        $body.html(_fmt_spellset_html(desc.body));
        _fmt_spells_list($body, desc.spellset, true);
        var s = scroller($body[0]);
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });
        if (desc.actions)
            $popup.find(".actions").html(clickify_actions(desc.actions));
        else
            $popup.find(".actions").remove();

        var canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas[0],
            renderer.cell_width * describe_scale,
            renderer.cell_height * describe_scale);
        renderer.init(canvas[0]);

        desc.tiles.forEach(function (tile) {
            renderer.draw_from_texture(tile.t, 0, 0, tile.tex, 0, 0, tile.ymax,
                false, describe_scale);
        });

        return $popup;
    }

    function format_spell_html(desc)
    {
        var parts = desc.match(/(^[\s\S]*\n\n)(Level: [^\n]+)(\n\n[\s\S]*)$/);
        if (parts == null || parts.length != 4)
            return fmt_body_txt(desc);
        parts[2] = parts[2].replace(/ /g, '&nbsp;');
        return fmt_body_txt(parts[1]) + parts[2] + fmt_body_txt(parts[3]);
    }

    function describe_spell(desc)
    {
        var $popup = $(".templates > .describe-spell").clone();
        $popup.find(".header > span").html(desc.title);
        $popup.find(".body").html(format_spell_html(desc.desc));
        var s = scroller($popup.find(".body")[0]);
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });
        if (desc.can_mem)
            $popup.find(".actions").removeClass("hidden");

        var canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas[0],
            renderer.cell_width * describe_scale,
            renderer.cell_height * describe_scale);
        renderer.init(canvas[0]);
        renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0,
            desc.tile.ymax, false, describe_scale);

        return $popup;
    }

    function describe_cards(desc)
    {
        var $popup = $(".templates > .describe-cards").clone();
        var $card_tmpl = $(".templates > .describe-generic");
        var t = gui.NEMELEX_CARD, tex = enums.texture.GUI;
        desc.cards.forEach(function (card) {
            var $card = $card_tmpl.clone().removeClass("hidden").addClass("describe-card");
            $card.find(".header > span").html(card.name);
            $card.find(".body").html(fmt_body_txt(card.desc));

            var canvas = $card.find(".header > canvas");
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas(canvas[0],
                renderer.cell_width * describe_scale,
                renderer.cell_height * describe_scale);
            renderer.init(canvas[0]);
            renderer.draw_from_texture(t, 0, 0, tex, 0, 0, 0, false,
                describe_scale);
            $popup.append($card);
        });
        var s = scroller($popup[0]);
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });
        return $popup;
    }

    function describe_god(desc)
    {
        var use_extra_pane = (desc.extra.length > 0);

        if (use_extra_pane)
            var $popup = $(".templates > #describe-god-extra").clone();
        else
            var $popup = $(".templates > #describe-god-basic").clone();
        $popup.find(".header > span").addClass("fg" + desc.colour).html(
                                                                    desc.name);

        var canvas = $popup.find(".header > canvas")[0];
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas,
            renderer.cell_width * describe_scale,
            renderer.cell_height * describe_scale);
        renderer.init(canvas);
        renderer.draw_from_texture(desc.tile.t, 0, 0,
                                   desc.tile.tex, 0, 0, 0, false,
                                   describe_scale);

        var $body = $popup.children(".body");
        var $footer = $popup.find(".footer > .paneset");
        var $panes = $body.children(".pane");

        if (!desc.is_altar)
            $footer.find('.join-keyhelp').remove();
        else
            $footer.find('.join-keyhelp').append(desc.service_fee)

        $panes.eq(0).find(".desc").html(desc.description);
        $panes.eq(0).find(".god-favour td.title")
                    .addClass("fg"+desc.colour).html(desc.title);
        $panes.eq(0).find(".god-favour td.favour")
                    .addClass("fg"+desc.colour).html(desc.favour);
        var powers_list = desc.powers_list.split("\n").slice(3, -1);
        var $powers = $panes.eq(0).find(".god-powers");
        var re = /^(<[a-z]*>)?(.*\.) *(\(.*\))?$/;
        for (var i = 0; i < powers_list.length; i++)
        {
            var matches = powers_list[i].match(re);
            var colour = (matches.length == 4 && matches[1] === "<darkgrey>")
                ? 8 : desc.colour;
            var power = matches[2], cost = matches[3] || "";
            $powers.append("<div class='power fg"+colour+"'><div>"
                +power+"</div><div>"+cost+"</div></div>");
        }

        desc.powers = fmt_body_txt(desc.powers);
        if (desc.info_table.length !== "")
        {
            desc.powers += "<div class=tbl>"
                            + util.formatted_string_to_html(desc.info_table)
                            + "</div>";
        }
        $panes.eq(1).html(desc.powers);

        $panes.eq(2).html(
                    fmt_body_txt(desc.wrath));
        if (use_extra_pane)
        {
            $panes.eq(3).html("<div class=tbl>"
                                + util.formatted_string_to_html(desc.extra)
                                + "</div>");
        }

        var num_panes = (use_extra_pane ? 3 : 2)
        for (var i = 0; i <= num_panes; i++)
            scroller($panes.eq(i)[0]);


        $popup.on("keydown keypress", function (event) {
            var enter = event.which == 13, space = event.which == 32;
            if (enter || space)
                return;
            var s = scroller($panes.filter(".current")[0]);
            scroller_handle_key(s, event);
        });

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

    function describe_item_spell_success(desc)
    {
        var $popup = ui.top_popup();
        if (desc.body !== undefined)
        {
            var $body = $popup.find(".simplebar-content");
            $body.html(fmt_body_txt(desc.body));
        }
    }

    function describe_monster(desc)
    {
        var $popup = $(".templates > .describe-monster").clone();
        $popup.find(".header > span").html(desc.title);
        var $body = $popup.find(".body.paneset");
        var $footer = $popup.find(".footer > .paneset");
        var $panes = $body.find(".pane");
        $panes.eq(0).html(_fmt_spellset_html(desc.body));
        _fmt_spells_list($panes.eq(0), desc.spellset, false);
        var have_quote = desc.quote !== "";

        if (have_quote)
            $panes.eq(1).html(_fmt_spellset_html(desc.quote));
        else
        {
            $footer.parent().remove();
            $panes.eq(1).remove();
        }

        var $canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas($canvas[0],
            renderer.cell_width * describe_scale,
            renderer.cell_height * describe_scale);
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
                        0, 0, xofs, yofs, doll_part[1], describe_scale);
            });
        }

        if ((desc.fg_idx >= player.MCACHE_START) && desc.mcache)
        {
            $.each(desc.mcache, function (i, mcache_part) {
                if (mcache_part) {
                    var yofs = Math.max(0, player.get_tile_info(mcache_part[0]).h - 32);
                    renderer.draw_player(mcache_part[0],
                            0, 0, mcache_part[1], mcache_part[2]+yofs,
                            undefined, describe_scale);
                }
            });
        }
        else if (desc.fg_idx > 0 && desc.fg_idx <= main.MAIN_MAX)
        {
            renderer.draw_foreground(0, 0, { t: {
                fg: { value: desc.fg_idx }, bg: 0, icons: [],
            }}, describe_scale);
        }

        renderer.draw_foreground(0, 0, { t: {
            fg: enums.prepare_fg_flags(desc.flag),
            bg: 0,
            icons: desc.icons,
        }}, describe_scale);

        for (var i = 0; i < $panes.length; i++)
            scroller($panes.eq(i)[0]);

        $popup.on("keydown keypress", function (event) {
            var s = scroller($panes.filter(".current")[0]);
            scroller_handle_key(s, event);
        });
        $popup.on("keydown", function (event) {
            if (event.key === "!")
            {
                paneset_cycle($body);
                if (have_quote)
                    paneset_cycle($footer);
            }
        });
        paneset_cycle($body);
        if (have_quote)
            paneset_cycle($footer);

        return $popup;
    }

    function describe_monster_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("describe-monster"))
            return;
        if (msg.pane !== undefined && client.is_watching())
        {
            paneset_cycle($popup.children(".body"), msg.pane);
            var $footer = $popup.find(".footer > .paneset");
            if ($footer.length > 0)
                paneset_cycle($footer, msg.pane);
        }
    }

    function version(desc)
    {
        var $popup = $(".templates > .describe-generic").clone();
        $popup.find(".header > span").html(desc.information);
        var $body = $popup.find(".body");
        $body.html(fmt_body_txt(desc.features) + fmt_body_txt(desc.changes));
        var s = scroller($body[0]);
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });

        var t = gui.STARTUP_STONESOUP, tex = enums.texture.GUI;
        var $canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas($canvas[0],
            renderer.cell_width * describe_scale,
            renderer.cell_height * describe_scale);
        renderer.init($canvas[0]);
        renderer.draw_from_texture(t, 0, 0, tex, 0, 0, 0, false,
            describe_scale);

        return $popup;
    }

    var update_server_scroll_timeout = null;
    var scroller_from_server = false;

    function scroller_handle_key(scroller, event)
    {
        if (event.altKey || event.shiftkey || event.ctrlKey) {
            if (update_server_scroll_timeout)
                update_server_scroll();
            return;
        }

        var handled = true;

        if (event.type === "keydown")
            switch (event.which)
            {
                case 33: // page up
                    scroller_scroll_page(scroller, -1);
                    break;
                case 34: // page down
                    scroller_scroll_page(scroller, 1);
                    break;
                case 35: // end
                    scroller_scroll_to_line(scroller, 2147483647); //int32_t max
                    break;
                case 36: // home
                    scroller_scroll_to_line(scroller, 0);
                    break;
                case 38: // up
                    scroller_scroll_line(scroller, -1);
                    break;
                case 40: // down
                    scroller_scroll_line(scroller, 1);
                    break;
                default:
                    handled = false;
                    break;
            }
        else if (event.type === "keypress")
            switch (String.fromCharCode(event.which))
            {
                case " ": case ">": case "+": case '\'':
                    scroller_scroll_page(scroller, 1);
                    break;
                case "-": case "<": case ";":
                    scroller_scroll_page(scroller, -1);
                    break;
                default:
                    handled = false;
                    break;
            }
        else
            handled = false;

        if (handled)
        {
            event.preventDefault();
            event.stopImmediatePropagation();
        }
        return handled;
    }

    function scroller_line_height(scroller)
    {
        var span = $(scroller.scrollElement).siblings('.scroller-lhd')[0];
        var rect = span.getBoundingClientRect();
        return rect.bottom - rect.top;
    }

    function scroller_scroll_page(scroller, dir)
    {
        var line_height = scroller_line_height(scroller);
        var contents = $(scroller.scrollElement);
        // XX this is a bit weird, maybe context[0] is the wrong thing to use?
        // The -24 is to compensate for the top/bottom shades. In practice, the
        // top line ends up a bit in the shade in long docs, possibly it should
        // be adjusted for.
        var page_shift = contents[0].getBoundingClientRect().height - 24;
        page_shift = Math.floor(page_shift / line_height) * line_height;
        contents[0].scrollTop += page_shift * dir;
        update_server_scroll();
    }

    function scroller_scroll_line(scroller, dir)
    {
        var line_height = scroller_line_height(scroller);
        var contents = $(scroller.scrollElement);
        contents[0].scrollTop += line_height * dir;
        update_server_scroll(scroller);
    }

    function scroller_scroll_to_line(scroller, line)
    {
        var $scroller = $(scroller.scrollElement);
        // TODO: can this work without special-casing int32_t max, where any
        // pos greater than the max scrolls to end?
        if (line == 2147483647) // FS_START_AT_END
        {
            // special case for webkit: excessively large values don't work
            var inner_h = $(scroller.contentElement).outerHeight();
            $scroller[0].scrollTop = inner_h - $scroller.parent().outerHeight();
        }
        else
        {
            var line_height = scroller_line_height(scroller);
            line *= line_height;
            $scroller[0].scrollTop = line;
        }
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
        var body = $popup.find(".simplebar-scroll-content").parent()[0];
        var s = scroller(body);
        var container = s.scrollElement;
        var line_height = scroller_line_height(s);
        comm.send_message("formatted_scroller_scroll", {
            scroll: Math.round(container.scrollTop / line_height),
        });
    }

    function scroller_onscroll()
    {
        // don't tell the server we scrolled, when we did so because the
        // server told us to; not absolutely necessary, but nice to have
        if (scroller_from_server)
            return;
        if (!update_server_scroll_timeout)
            update_server_scroll_timeout = setTimeout(update_server_scroll, 100);
    }

    function formatted_scroller(desc)
    {
        var $popup = $(".templates > .formatted-scroller").clone();
        var $body = $popup.children(".body");
        var body_html = util.formatted_string_to_html(desc.text);
        if (desc.highlight !== "")
        {
            var rexp = '[^\n]*('+desc.highlight+')[^\n]*\n?';
            var re = new RegExp(rexp, 'g');
            body_html = body_html.replace(re, function (line) {
                return "<span class=highlight>" + line + "</span>";
            });
        }
        $popup.attr("data-tag", desc.tag);
        $body.html(body_html);
        if (desc.more)
            $popup.children(".more").html(util.formatted_string_to_html(desc.more));
        else
            $popup.children(".more").remove();

        // XX why do some layouts use a span inside of a div, some just use
        // the div? (Answer remains unclear to me, but something about
        // preformatting + nested spans did mess this up when I tried to use
        // a template span here)
        if (desc.title)
            $popup.children(".header").html(util.formatted_string_to_html(desc.title));
        else
            $popup.children(".header").remove();

        var s = scroller($body[0]);
        var scroll_elem = s.scrollElement;
        scroll_elem.addEventListener("scroll", scroller_onscroll);
        $popup.on("keydown keypress", function (event) {
            if (event.which !== 36 || desc.tag !== "help")
                scroller_handle_key(s, event);
        });
        if (desc.easy_exit && options.get("tile_web_mouse_control"))
        {
            $popup.on("click", function () {
                // XX a bit ad hoc
                comm.send_message("key", { keycode: 27 });
            });
        }
        return $popup;
    }

    function formatted_scroller_update(msg)
    {
        var $popup = ui.top_popup();
        if (!$popup.hasClass("formatted-scroller"))
            return;
        var s = scroller($popup.find(".body")[0]);
        if (msg.text !== undefined)
        {
            var $body = $(s.contentElement);
            $body.html(util.formatted_string_to_html(msg.text));
            var ss = $popup.find(".body").eq(0).data("scroller");
            ss.recalculateImmediate();
        }
        if (msg.scroll !== undefined && (!msg.from_webtiles || client.is_watching()))
        {
            scroller_from_server = true;
            scroller_scroll_to_line(s, msg.scroll);
            scroller_from_server = false;
        }
    }

    function msgwin_get_line(msg)
    {
        var $popup = $(".templates > .msgwin-get-line").clone();
        $popup.children(".header").html(util.formatted_string_to_html(msg.prompt));
        return $popup;
    }

    function focus_button($button)
    {
        var $popup = $button.closest(".newgame-choice");
        var menu_id = $button.closest("[menu_id]").attr("menu_id");
        $popup.find(".button.selected").removeClass("selected");
        $button.addClass("selected");
        var $scr = $button.closest(".simplebar-scroll-content");
        if ($scr.length === 1)
        {
            var br = $button[0].getBoundingClientRect();
            var gr = $scr.parent()[0].getBoundingClientRect();
            var delta = br.top < gr.top ? br.top - gr.top :
                    br.bottom > gr.bottom ? br.bottom - gr.bottom : 0;
            $scr[0].scrollTop += delta;
        }

        var $descriptions = $popup.find(".descriptions");
        paneset_cycle($descriptions, $button.attr("data-description-index"));
        comm.send_message("outer_menu_focus", {
            hotkey: ui.utf8_from_key_value($button.attr("data-hotkey")),
            menu_id: menu_id,
        });
    }

    function newgame_choice(msg)
    {
        var $popup = $(".templates > .newgame-choice").clone();
        if (msg.doll)
        {
            var $canvas = $("<canvas>");
            var $title = $("<span>");
            var $prompt = $("<div class=header>");
            $popup.children(".header").append($canvas).append($title).after($prompt);
            $title.html(util.formatted_string_to_html(msg.title));
            $prompt.html(util.formatted_string_to_html(msg.prompt));
            var renderer = new cr.DungeonCellRenderer();
            util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
            renderer.init($canvas[0]);
            $.each(msg.doll, function (i, doll_part) {
                renderer.draw_player(doll_part[0], 0, 0, 0, 0, doll_part[1]);
            });
        }
        else
            $popup.children(".header").html(util.formatted_string_to_html(msg.title));
        var renderer = new cr.DungeonCellRenderer();
        var $descriptions = $popup.find(".descriptions");

        function build_item_grid(data, $container, fat)
        {
            $container.attr("menu_id", data.menu_id);
            $.each(data.buttons, function (i, button) {
                var $button = $("<div class=button>");
                if ((button.tile || []).length > 0 || fat)
                {
                    var $canvas = $("<canvas class='glyph-mode-hidden'>");
                    util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
                    renderer.init($canvas[0]);

                    $.each(button.tile || [], function (_, tile) {
                        renderer.draw_from_texture(tile.t, 0, 0, tile.tex, 0, 0, tile.ymax, false);
                    });
                    $button.append($canvas);
                }
                $.each(button.labels || [button.label], function (i, label) {
                    // TODO: this has somewhat weird behavior if multiple spans
                    // are produced from the formatted string...should they
                    // really have a flex-grow property?
                    var $lbl = $(util.formatted_string_to_html(label)).css("flex-grow", "1");
                    $button.append($lbl);
                });
                // at least remove empty spans because of the above issue:
                $button.find("span:empty").remove();
                $button.attr("style", "grid-row:"+(button.y+1)+"; grid-column:"+(button.x+1)+";");
                $descriptions.append("<span class='pane'> " + button.description + "</span>");
                $button.attr("data-description-index", $descriptions.children().length - 1);
                $button.attr("data-hotkey", ui.key_value_from_utf8(button.hotkey));
                $button.addClass("hlc-" + button.highlight_colour);
                $container.append($button);

                $button.on('hover', function () { focus_button($(this)) });
            });
            $.each(data.labels, function (i, button) {
                var $button = $("<div class=label>");
                $button.append(util.formatted_string_to_html(button.label));
                $button.attr("style", "grid-row:"+(button.y+1)+"; grid-column:"+(button.x+1)+";");
                $container.append($button);
            });
        }

        var $main = $popup.find(".main-items");
        build_item_grid(msg["main-items"], $main, true);
        build_item_grid(msg["sub-items"], $popup.find(".sub-items"));
        scroller($main.parent()[0]);

        return $popup;
    }

    function newgame_choice_update(msg)
    {
        if (!client.is_watching() && msg.from_client)
            return;
        var $popup = ui.top_popup();
        if (!$popup || !$popup.hasClass("newgame-choice"))
            return;
        var hotkey = ui.key_value_from_utf8(msg.button_focus);
        focus_button($popup.find("[data-hotkey='"+hotkey+"']"));
    }

    function newgame_random_combo(msg)
    {
        var $popup = $(".templates > .describe-generic").clone();
        $popup.find(".header > span").html(util.formatted_string_to_html(msg.prompt));
        $popup.find(".body").html("Do you want to play this combination? [Y/n/q]");

        var $canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas($canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init($canvas[0]);
        $.each(msg.doll, function (i, doll_part) {
            renderer.draw_player(doll_part[0], 0, 0, 0, 0, doll_part[1]);
        });

        return $popup;
    }

    function game_over(desc)
    {
        var $popup = $(".templates > .game-over").clone();
        $popup.find(".header > span").html(desc.title);
        $popup.children(".body").html(fmt_body_txt(desc.body));
        var s = scroller($popup.children(".body")[0]);
        $popup.on("keydown keypress", function (event) {
            scroller_handle_key(s, event);
        });

        var canvas = $popup.find(".header > canvas");
        var renderer = new cr.DungeonCellRenderer();
        util.init_canvas(canvas[0], renderer.cell_width, renderer.cell_height);
        renderer.init(canvas[0]);
        renderer.draw_from_texture(desc.tile.t, 0, 0, desc.tile.tex, 0, 0, desc.tile.ymax, false);

        return $popup;
    }

    function seed_selection(desc)
    {
        var $popup = $(".templates > .seed-selection").clone();
        $popup.find(".header").html(desc.title);
        $popup.find(".body-text").html(desc.body);
        $popup.find(".footer").html(desc.footer);
        if (!desc.show_pregen_toggle)
            $popup.find(".pregen-toggle").remove();
        scroller($popup.find(".body")[0]);

        var $input = $popup.find("input[type=text]");
        var input_val = $input.val();
        $input.on("input change", function (event) {
            var valid_seed = $input.val().match(/^\d*$/);
            if (valid_seed)
                input_val = $input.val();
            else
                $input.val(input_val);
        });

        return $popup;
    }

    var ui_handlers = {
        "describe-generic" : describe_generic,
        "describe-feature-wide" : describe_feature_wide,
        "describe-item" : describe_item,
        "describe-spell" : describe_spell,
        "describe-cards" : describe_cards,
        "describe-god" : describe_god,
        "describe-monster" : describe_monster,
        "version" : version,
        "formatted-scroller" : formatted_scroller,
        "progress-bar" : progress_bar,
        "seed-selection" : seed_selection,
        "msgwin-get-line" : msgwin_get_line,
        "newgame-choice": newgame_choice,
        "newgame-random-combo": newgame_random_combo,
        "game-over" : game_over,
    };

    function register_ui_handlers(dict)
    {
        $.extend(ui_handlers, dict);
    }

    function recv_ui_push(msg)
    {
        var popup = null
        var handler = ui_handlers[msg.type];
        try
        {
            popup = handler
                ? handler(msg)
                : $("<div>Unhandled UI type " + msg.type + "</div>");
        }
        catch (err)
        {
            console.log(err);
            popup = $("<div>Buggy UI of type " + msg.type + "</div>");
        }
        ui.show_popup(popup, msg["ui-centred"], msg.generation_id);
    }

    function recv_ui_pop(msg)
    {
        ui.hide_popup();
    }

    var ui_stack_handled = false;

    function recv_ui_stack(msg)
    {
        if (!client.is_watching())
            return;
        // only process once on load
        if (ui_stack_handled)
            return;

        for (var i = 0; i < msg.items.length; i++)
            comm.handle_message(msg.items[i]);
        ui_stack_handled = true;
    }

    function recv_ui_state(msg)
    {
        var ui_handlers = {
            "describe-god" : describe_god_update,
            "describe-monster" : describe_monster_update,
            "formatted-scroller" : formatted_scroller_update,
            "progress-bar" : progress_bar_update,
            "newgame-choice" : newgame_choice_update,
            "describe-spell-success" : describe_item_spell_success,
        };
        var handler = ui_handlers[msg.type];
        if (handler)
            handler(msg);
    }

    function recv_ui_scroll(msg)
    {
        var $popup = ui.top_popup();
        // formatted scrollers send their own synchronization messages
        if ($popup === undefined
            || $popup.hasClass("formatted-scroller")
            || $popup.hasClass("menu"))
            return;

        if (msg.scroll !== undefined && (!msg.from_webtiles || client.is_watching()))
        {
            var body = $popup.find(".simplebar-scroll-content").parent();
            if (body.length === 1)
                scroller_scroll_to_line(scroller(body[0]), msg.scroll);
        }
    }

    function ui_layouts_cleanup()
    {
        ui_stack_handled = false;
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
        "ui-scroller-scroll": recv_ui_scroll,
    });

    return {
        register_handlers: register_ui_handlers,
    };
});
