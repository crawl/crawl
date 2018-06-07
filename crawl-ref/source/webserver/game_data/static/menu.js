define(["jquery", "comm", "client", "./enums", "./cell_renderer",
        "./util", "./options"],
function ($, comm, client, enums, cr, util, options) {
    "use strict";

    var chunk_size = 50;

    // Helpers

    function item_selectable(item)
    {
        return item.level == 2 && item.hotkeys && item.hotkeys.length;
    }

    function item_text(item)
    {
        return item.text;
    }

    function item_colour(item)
    {
        return item.colour || 7;
    }

    function menu_title_indent()
    {
        if (!options.get("tile_menu_icons")
            || !(menu.tag === "ability" || menu.tag === "spell"))
            return 0;
        return 32 + 2; // menu <ol> has a 2px margin
    }

    function set_item_contents(item, elem)
    {
        elem.html(util.formatted_string_to_html(item_text(item)));
        var col = item_colour(item);
        elem.removeClass();
        elem.addClass("level" + item.level);
        elem.addClass("fg" + col);

        if (item.level < 2)
            elem.css("padding-left", menu_title_indent()+"px");

        if (item_selectable(item))
        {
            elem.addClass("selectable");
            elem.off("click.menu_item");
            elem.on("click.menu_item", item_click_handler);
        }

        if (item.tiles && item.tiles.length > 0
            && options.get("tile_display_mode") == "tiles")
        {
            var renderer = new cr.DungeonCellRenderer();
            var canvas = $("<canvas>");
            util.init_canvas(canvas[0], renderer.cell_width,
                                        renderer.cell_height);
            canvas.css("vertical-align", "middle");
            renderer.init(canvas[0]);

            $.each(item.tiles, function () {
                renderer.draw_from_texture(this.t, 0, 0, this.tex, 0, 0, this.ymax, false);
            });

            elem.prepend(canvas);
        }
    }

    var menu_stack = [];
    var menu = null;
    var update_server_scroll_timeout = null;
    var menu_close_timeout = null;

    function menu_cleanup()
    {
        menu_stack = [];
        menu = null;
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
        if (menu_close_timeout)
        {
            clearTimeout(menu_close_timeout);
            menu_close_timeout = null;
        }
    }

    function display_menu()
    {
        var menu_div = $("<div>");
        menu_div.addClass("menu_" + menu.tag);
        menu.elem = menu_div;

        $("#menu").html(menu_div);

        if (menu.type === "crt")
        {
            // Custom-drawn CRT menu
            menu_div.attr("id", "menu_txt");
            client.show_dialog("#menu");
            menu_div.bind("text_update", function () {
                client.center_element($("#menu"));
            });
            return;
        }

        // Normal menu
        menu_div.prepend("<div id='menu_title'>");
        update_title();

        var content_div= $("<div id='menu_contents'>");
        content_div.css({
            "max-height": $(window).height() - 100
        });
        menu_div.append(content_div);

        var items_inner = $("<div id='menu_contents_inner'>");
        content_div.append(items_inner);

        var container = $("<ol>");
        items_inner.append(container);

        if (!menu.created)
        {
            var chunk = menu.items;
            menu.items = { length: menu.total_items };
            menu.first_present = 999999;
            menu.last_present = -999999;
            prepare_item_range(menu.chunk_start,
                               menu.chunk_start + chunk.length - 1,
                               true, container);
            update_item_range(menu.chunk_start, chunk);
        }
        else
        {
            for (var i = menu.first_present; i <= menu.last_present; ++i)
            {
                var item = menu.items[i];
                if (!item) continue;
                item.elem = $("<li>...</li>");
                item.elem.data("item", item);
                item.elem.addClass("placeholder");
                container.append(item.elem);
                set_item_contents(item, item.elem);
            }
        }

        menu_div.append("<div id='menu_more'>" + util.formatted_string_to_html(menu.more)
                        + "</div>");

        content_div.scroll(menu_scroll_handler);

        menu.server_scroll = true;
        menu.created = true;

        client.show_dialog("#menu");

        if (menu.flags & enums.menu_flag.START_AT_END)
        {
            scroll_bottom_to_item(menu.items.length - 1, true);
        }
        else if (menu.jump_to)
        {
            scroll_to_item(menu.jump_to, true);
        } else if (menu.items.length > 0) {
            scroll_to_item(0, true);
        }
    }

    function prepare_item_range(start, end, no_request, container)
    {
        // Guarantees that the given (inclusive) range of item indices
        // exists; requests them from the server if necessary, except
        // if no_request is true.

        if (start < 0) start = 0;
        if (end >= menu.total_items)
            end = menu.total_items - 1;

        if (start >= menu.total_items || end < 0)
            return;

        // Find out which indices are missing. This assumes that all
        // missing items are in a continuous range, which is the case
        // as long as the only ways to jump farther than chunk_size are
        // home and end.
        while (menu.items[start] !== undefined && start <= end)
            start++;
        if (start > end) return;
        while (menu.items[end] !== undefined && start <= end)
            end--;

        // Extend to chunk_size, but only if we are actually
        // requesting items (otherwise we would end up with
        // placeholders for which items are never requested).
        if (!no_request)
        {
            while (end - start + 1 < chunk_size
                   && menu.items[end + 1] === undefined
                   && end < menu.total_items - 1)
            {
                end++;
            }
            while (end - start + 1 < chunk_size
                   && menu.items[start - 1] === undefined
                   && start > 0)
            {
                start--;
            }
        }

        container = container || $("#menu_contents_inner ol");

        // Find the place where we add the new elements
        var present_index = end;
        while (present_index < menu.total_items
               && menu.items[present_index] === undefined)
        {
            present_index++;
        }
        var anchor = null;
        if (menu.items[present_index])
            anchor = menu.items[present_index].elem;

        // Create the placeholders
        for (var i = start; i <= end; ++i)
        {
            var item = {
                level: 2,
                text: "...",
                index: i
            };
            var elem = $("<li>...</li>");
            elem.data("item", item);
            elem.addClass("placeholder");
            item.elem = elem;

            if (anchor)
                anchor.before(elem);
            else
                container.append(elem);

            menu.items[i] = item;
        }

        if (start < menu.first_present)
            menu.first_present = start;
        if (end > menu.last_present)
            menu.last_present = end;

        // Request the actual elements
        if (!no_request)
            comm.send_message("*request_menu_range", { start: start, end: end });
    }

    function update_item_range(chunk_start, items_list)
    {
        for (var i = 0; i < items_list.length; ++i)
        {
            var real_index = i + chunk_start;
            var item = menu.items[real_index];
            if (!item) continue;
            var new_item = items_list[i];
            if (typeof new_item === "string")
            {
                new_item = {
                    type: 2,
                    text: new_item
                };
            }
            $.extend(item, new_item);
            if (new_item.colour === undefined)
                delete item.colour;

            set_item_contents(item, item.elem);
        }
    }

    // Scrolling functions

    function page_down()
    {
        var next = menu.last_visible + 1;
        if (next >= menu.items.length)
            next = menu.items.length - 1;
        scroll_to_item(next);
    }

    function page_up()
    {
        var previous = menu.first_visible - 1;
        if (previous < 0)
            previous = 0;
        scroll_bottom_to_item(previous);
    }

    function line_down()
    {
        var next = menu.first_visible + 1;
        if (next >= menu.items.length)
            next = menu.items.length - 1;
        scroll_to_item(next);
    }

    function line_up()
    {
        var previous = menu.first_visible - 1;
        if (previous < 0)
            previous = 0;
        scroll_to_item(previous);
    }

    function scroll_to_item(item_or_index, was_server_initiated)
    {
        var index = (item_or_index.elem ?
                     item_or_index.index : item_or_index);
        if (menu.first_visible == index) return;

        prepare_item_range(index, index + chunk_size - 1);

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $("#menu_contents");
        var baseline = contents.children().offset().top;

        contents.scrollTop(item.elem.offset().top - baseline);

        menu.anchor_last = false;
        if (!was_server_initiated)
            menu.server_scroll = false;
        menu_scroll_handler();
    }

    function scroll_bottom_to_item(item_or_index, was_server_initiated)
    {
        var index = (item_or_index.elem ?
                     item_or_index.index : item_or_index);
        if (menu.last_visible == index) return;

        prepare_item_range(index - chunk_size + 1, index);

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $("#menu_contents");
        var baseline = contents.children().offset().top;

        contents.scrollTop(item.elem.offset().top + item.elem.height()
                           - baseline - contents.innerHeight());

        menu.anchor_last = true;
        if (!was_server_initiated)
            menu.server_scroll = false;
        menu_scroll_handler();
    }

    function update_visible_indices()
    {
        var contents = $("#menu_contents");
        var top = contents.offset().top;
        var bottom = top + contents.innerHeight();
        menu.first_visible = null;
        menu.last_visible = null;
        for (var i in menu.items)
        {
            if (!menu.items.hasOwnProperty(i) || i == "length") continue;
            var item = menu.items[i];
            var item_top = item.elem.offset().top;
            var item_bottom = item_top + item.elem.outerHeight();
            if (item_top <= top && item_bottom >= top)
            {
                var candidate = Number(i) + 1;
                while (menu.items[candidate] == null &&
                       candidate <= menu.last_present)
                {
                    candidate++;
                }
                menu.first_visible = candidate;
                if (menu.last_visible !== null) return;
            }
            if (item_top <= bottom && item_bottom >= bottom)
            {
                var candidate = Number(i) - 1;
                while (menu.items[candidate] == null &&
                       candidate >= menu.first_present)
                {
                    candidate--;
                }
                menu.last_visible = candidate;
                if (menu.first_visible !== null) return;
            }
        }
        menu.first_visible = menu.first_visible || menu.first_present;
        menu.last_visible = menu.last_visible || menu.last_present;
    }

    function update_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }

        if (!menu) return;

        comm.send_message("menu_scroll", {
            first: menu.first_visible,
            last: menu.last_visible
        });
    }

    function schedule_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }
        update_server_scroll_timeout = setTimeout(update_server_scroll, 500);
    }

    function update_title()
    {
        $("#menu_title").html(util.formatted_string_to_html(menu.title.text));
        $("#menu_title").css("padding-left", menu_title_indent()+"px");
    }

    function pattern_select()
    {
        var title = $("#menu_title");
        title.html("Select what? (regex) ");
        var input = $("<input class='text pattern_select' type='text'>");
        title.append(input);

        if (!client.is_watching || !client.is_watching())
            input.focus();

        var restore = function () {
            if (!client.is_watching || !client.is_watching())
                input.blur();
            update_title();
        };

        input.keydown(function (ev) {
            if (ev.which == 27)
            {
                restore(); // ESC
                ev.preventDefault();
                return false;
            }
            else if (ev.which == 13)
            {
                var ctrlf = String.fromCharCode(6);
                var enter = String.fromCharCode(13);
                var text = ctrlf + input.val() + enter;
                comm.send_message("input", { text: text });

                restore();
                ev.preventDefault();
                return false;
            }
        });
    }

    // Message handlers

    function open_menu(data)
    {
        if (data.replace) menu_stack.pop();
        menu_stack.push(data);
        menu = data;

        display_menu();
    }

    function close_menu()
    {
        menu_stack.pop();

        if (menu_stack.length == 0)
        {
            menu = null;
            // Delay closing the dialog a bit to prevent flickering
            // if the game immediately opens another one
            // (e.g. when looking at an item from the inventory)
            if (menu_close_timeout)
                clearTimeout(menu_close_timeout);
            menu_close_timeout = setTimeout(function () {
                menu_close_timeout = null;
                if (menu_stack.length == 0)
                    client.hide_dialog();
            }, 50);
        }
        else
        {
            menu = menu_stack[menu_stack.length - 1];
            display_menu();
        }
    }

    function close_all_menus()
    {
        while (menu_stack.length > 0)
            close_menu();
    }

    function update_menu(data)
    {
        $.extend(menu, data);

        var old_length = menu.items.length;
        menu.items.length = menu.total_items;
        if (menu.total_items < old_length)
        {
            for (var i = old_length; i >= menu.total_items; --i)
                delete menu.items[i];
            var container = $("ol");
            container.empty();
            $.each(menu.items, function(i, item) {
                item.elem.data("item", item);
                container.append(item.elem);
            });
        }
        update_title();
        $("#menu_more").html(util.formatted_string_to_html(menu.more));

        client.center_element($("#menu"));
    }

    function update_menu_items(data)
    {
        prepare_item_range(data.chunk_start,
                           data.chunk_start + data.items.length - 1,
                           true);

        update_item_range(data.chunk_start, data.items);

        handle_size_change();
    }

    function server_menu_scroll(data)
    {
        menu.server_first_visible = data.first;
        if (menu.server_scroll)
            scroll_to_item(data.first, true);
    }

    function init_menus(data)
    {
        if (menu_stack.length > 0) return;

        menu_stack = data.menus;
        if (menu_stack.length > 0)
        {
            menu = menu_stack[menu_stack.length - 1];
            display_menu();
        }
        else
            menu = null;
    }

    comm.register_handlers({
        "menu": open_menu,
        "close_menu": close_menu,
        "close_all_menus": close_all_menus,
        "update_menu": update_menu,
        "update_menu_items": update_menu_items,
        "menu_scroll": server_menu_scroll,
        "init_menus": init_menus
    });

    // Event handlers

    function handle_size_change()
    {
        if (!menu) return;

        var items = $("#menu_contents");
        items.css({
            "max-height": $(window).height() - 100
        });
        client.center_element($("#menu"));
        if (menu.anchor_last)
            scroll_bottom_to_item(menu.last_visible, true);
        else if (menu.first_visible)
            scroll_to_item(menu.first_visible, true);
        // scrolling might not call this, but still need to show/hide more
        menu_scroll_handler();
    }

    function menu_scroll_handler()
    {
        var contents = $("#menu_contents");
        update_visible_indices();
        schedule_server_scroll();
        if (menu.last_visible >= menu.items.length - 1
            && !(menu.flags & enums.menu_flag.ALWAYS_SHOW_MORE))
        {
            $("#menu_more").css("visibility", "hidden");
        }
        else
        {
            $("#menu_more").css("visibility", "visible");
        }
    }

    function menu_keydown_handler(event)
    {
        if (!menu || menu.type === "crt") return;

        if (event.altKey || event.shiftkey) {
            if (update_server_scroll_timeout)
                update_server_scroll();
            return;
        }

        if (event.ctrlKey)
        {
            if (String.fromCharCode(event.which) == "F")
            {
                if ((menu.flags & enums.menu_flag.ALLOW_FILTER))
                {
                    pattern_select();
                }
                event.preventDefault();
                return false;
            }
            if (update_server_scroll_timeout)
                update_server_scroll();
            return;
        }

        switch (event.which)
        {
        case 33: // page up
            page_up();
            event.preventDefault();
            return false;
        case 34: // page down
            page_down();
            event.preventDefault();
            return false;
        case 35: // end
            scroll_bottom_to_item(menu.total_items - 1);
            event.preventDefault();
            return false;
        case 36: // home
            if (menu.tag !== "help")
            {
                scroll_to_item(0);
                event.preventDefault();
                return false;
            }
            else break;
        case 38: // up
            line_up();
            event.preventDefault();
            return false;
        case 40: // down
            line_down();
            event.preventDefault();
            return false;
        }

        if (update_server_scroll_timeout)
            update_server_scroll();
    }

    function menu_keypress_handler(event)
    {
        if (!menu || menu.type === "crt") return;

        var chr = String.fromCharCode(event.which);

        switch (chr)
        {
        case "-":
            if (menu.tag == "inventory" || menu.tag == "stash")
                break; // Don't capture - for wield prompts or stash search

        case "<":
        case ";":
            page_up();
            event.preventDefault();
            return false;

        case ">":
        case "+":
        case " ":
            page_down();
            event.preventDefault();
            return false;
        }

        if (update_server_scroll_timeout)
            update_server_scroll();
    }

    function item_click_handler(event)
    {
        if (update_server_scroll_timeout)
            update_server_scroll();

        var item = $(this).data("item");
        if (!item) return;
        if (item.hotkeys && item.hotkeys.length)
            comm.send_message("input", { data: [ item.hotkeys[0] ] });
    }

    options.add_listener(function ()
    {
        if (options.get("tile_font_crt_size") === 0)
        {
            $("#crt").css("font-size", "");
            $("#menu").css("font-size", "");
        }
        else
        {
            $("#crt").css("font-size",
                options.get("tile_font_crt_size") + "px");
            $("#menu").css("font-size",
                options.get("tile_font_crt_size") + "px");
        }

        var family = options.get("tile_font_crt_family");
        if (family !== "" && family !== "monospace")
        {
            family += ", monospace";
            $("#crt").css("font-family", family);
            $("#menu").css("font-family", family);
        }

        handle_size_change();
    });

    $(document).off("game_init.menu")
               .on("game_init.menu", function () {
        menu_stack = [];
        $(window).off("resize.menu")
                 .on("resize.menu", handle_size_change);
        $(document).off("game_keydown.menu game_keypress.menu")
                   .on("game_keydown.menu", menu_keydown_handler)
                   .on("game_keypress.menu", menu_keypress_handler);
        $(document).off("game_cleanup_menu")
                   .on("game_cleanup.menu", menu_cleanup);
    });
});
