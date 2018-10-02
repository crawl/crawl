define(["jquery", "comm", "client", "./ui", "./enums", "./cell_renderer",
        "./util", "./options", "./scroller"],
function ($, comm, client, ui, enums, cr, util, options, scroller) {
    "use strict";

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
            || options.get("tile_display_mode") !== "tiles"
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
        var menu_div = $(".templates > .menu").clone();
        menu_div.addClass("menu_" + menu.tag);
        menu.elem = menu_div;

        if (menu.type === "crt")
        {
            // Custom-drawn CRT menu
            menu_div.removeClass("menu").addClass("menu_txt");
            ui.show_popup(menu_div, menu["ui-centred"]);
            return;
        }

        // Normal menu
        menu_div.prepend("<div class='menu_title'>");
        update_title();

        var content_div= $("<div class='menu_contents'>");
        menu_div.append(content_div);

        var items_inner = $("<div class='menu_contents_inner'>");
        content_div.append(items_inner);

        var container = $("<ol>");
        items_inner.append(container);

        var chunk = menu.items;
        menu.items = { length: menu.total_items };
        menu.first_present = 999999;
        menu.last_present = -999999;
        update_item_range(menu.chunk_start, chunk);

        menu.scroller = scroller(content_div[0]);
        menu.scroller.scrollElement.addEventListener('scroll', menu_scroll_handler);

        menu_div.append("<div class='menu_more'>" + util.formatted_string_to_html(menu.more)
                        + "</div>");

        if (client.is_watching())
            menu.following_player_scroll = true;

        ui.show_popup(menu_div, menu["ui-centred"]);
        handle_size_change();

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

    function prepare_item_range(start, end, container)
    {
        // Guarantees that the given (inclusive) range of item indices
        // exists

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

        container = container || menu.elem.find(".menu_contents_inner ol");

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
    }

    function update_item_range(chunk_start, items_list)
    {
        prepare_item_range(0, menu.total_items-1);
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
        var pagesz = menu.elem.find(".menu_contents").innerHeight();
        var itemsz = menu.items[menu.first_visible].elem[0].getBoundingClientRect().height;
        var delta = Math.floor(pagesz/itemsz)
        var previous = menu.first_visible - delta;
        if (previous < 0)
            previous = 0;
        scroll_to_item(previous);
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

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $(menu.scroller.scrollElement);
        var baseline = contents.children()[0].getBoundingClientRect().top;
        var elem_y = item.elem[0].getBoundingClientRect().top;
        contents[0].scrollTop = elem_y - baseline;

        menu.anchor_last = false;
        menu_scroll_handler(was_server_initiated);
    }

    function scroll_bottom_to_item(item_or_index, was_server_initiated)
    {
        var index = (item_or_index.elem ?
                     item_or_index.index : item_or_index);
        if (menu.last_visible == index) return;

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $(menu.scroller.scrollElement);
        var baseline = contents.children().offset().top;

        contents.scrollTop(item.elem.offset().top + item.elem.height()
                - baseline - menu.elem.find(".menu_contents").innerHeight());

        menu.anchor_last = true;
        menu_scroll_handler(was_server_initiated);
    }

    function update_visible_indices()
    {
        var $contents = menu.elem.find(".menu_contents");
        var container_rect = $contents.children()[0].getBoundingClientRect();
        var top = Math.max(container_rect.top, 0);
        var bottom = Math.min(container_rect.bottom,
                                $(window).scrollTop() + $(window).height());
        var i;

        // initialize these to ensure that they are never NaN, even if we have
        // strange values for the bounding boxes
        menu.first_visible = 0;
        menu.last_visible = 0;

        for (i = 0; i < menu.items.length; i++)
        {
            var item = menu.items[i];
            var item_top = item.elem[0].getBoundingClientRect().top;
            if (item_top >= top)
            {
                menu.first_visible = i;
                break;
            }
        }
        for (; i < menu.items.length; i++)
        {
            var item = menu.items[i];
            var item_bottom = item.elem[0].getBoundingClientRect().bottom;
            if (item_bottom >= bottom)
            {
                menu.last_visible = i-1;
                break;
            }
        }
    }

    function update_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }

        if (!menu) return;

        update_visible_indices();
        comm.send_message("menu_scroll", {
            first: menu.first_visible,
            last: menu.last_visible
        });
    }

    function schedule_server_scroll()
    {
        if (!update_server_scroll_timeout)
            update_server_scroll_timeout = setTimeout(update_server_scroll, 100);
    }

    function update_title()
    {
        var title = menu.elem.find(".menu_title")
        title.html(util.formatted_string_to_html(menu.title.text));
        title.css("padding-left", menu_title_indent()+"px");
    }

    function pattern_select()
    {
        var title = menu.elem.find(".menu_title")
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
        if (data.replace)
        {
            menu_stack.pop();
            ui.hide_popup();
        }
        menu_stack.push(data);
        menu = data;

        display_menu();
    }

    function close_menu()
    {
        menu_stack.pop();
        ui.hide_popup();
        menu = menu_stack[menu_stack.length - 1];
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
        menu.elem.find(".menu_more").html(util.formatted_string_to_html(menu.more));
    }

    function update_menu_items(data)
    {
        update_item_range(data.chunk_start, data.items);
        handle_size_change();
    }

    function server_menu_scroll(data)
    {
        if (!client.is_watching())
            return;
        menu.server_first_visible = data.first;
        if (menu.following_player_scroll)
            scroll_to_item(data.first, true);
    }

    comm.register_handlers({
        "menu": open_menu,
        "close_menu": close_menu,
        "close_all_menus": close_all_menus,
        "update_menu": update_menu,
        "update_menu_items": update_menu_items,
        "menu_scroll": server_menu_scroll,
    });

    // Event handlers

    function handle_size_change()
    {
        if (!menu) return;

        if (menu.anchor_last)
            scroll_bottom_to_item(menu.last_visible, true);
        else if (menu.first_visible)
            scroll_to_item(menu.first_visible, true);

        if (menu.type != "crt" && !(menu.flags & enums.menu_flag.ALWAYS_SHOW_MORE))
        {
            var contents_height = menu.elem.find(".menu_contents_inner").height();
            var contents = $(menu.scroller.scrollElement);
            var more = menu.elem.find(".menu_more");
            var avail_height = contents.height() + more.height();
            more[0].classList.toggle("hidden", contents_height <= avail_height);
        }
    }

    function menu_scroll_handler(was_server_initiated)
    {
        // XXX: punt on detecting user-initiated scrolling for now
        if (was_server_initiated === false)
            menu.following_player_scroll = false;

        update_visible_indices();
        schedule_server_scroll();
    }

    function menu_keydown_handler(event)
    {
        if (!menu || menu.type === "crt") return;
        // can't check `hidden` class, which is on a 3x containing div
        if (ui.top_popup().is(":hidden")) return;
        if (ui.top_popup()[0] !== menu.elem[0]) return;

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
            scroll_to_item(0);
            event.preventDefault();
            return false;
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
        if (ui.top_popup()[0] !== menu.elem[0]) return;

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
            $(".menu").css("font-size", "");
        }
        else
        {
            $("#crt").css("font-size",
                options.get("tile_font_crt_size") + "px");
            $(".menu").css("font-size",
                options.get("tile_font_crt_size") + "px");
        }

        var family = options.get("tile_font_crt_family");
        if (family !== "" && family !== "monospace")
        {
            family += ", monospace";
            $("#crt").css("font-family", family);
            $(".menu").css("font-family", family);
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
