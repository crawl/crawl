define(["jquery", "comm", "client", "./ui", "./enums", "./cell_renderer",
        "./util", "./options", "./scroller"],
function ($, comm, client, ui, enums, cr, util, options, scroller) {
    "use strict";

    // Helpers

    function item_selectable(item)
    {
        // TODO: the logic on the c++ side is somewhat different here
        return item.level == 2
            // in the use item menu, selecting a non-hotkeyed item triggers
            // relettering on the server
            && (menu.tag == "use_item" || item.hotkeys && item.hotkeys.length);
    }

    function item_text(item)
    {
        return item.text;
    }

    function item_colour(item)
    {
        if (item.colour === undefined)
            return 7;
        return item.colour;
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
        elem.css('min-height', '0.5em');
        var col = item_colour(item);
        elem.removeClass();
        elem.addClass("level" + item.level);
        elem.addClass("fg" + col);

        if (item.level < 2)
            elem.css("padding-left", menu_title_indent()+"px");

        if (item_selectable(item))
        {
            elem.addClass("selectable");
            elem.off("click.menu_item").off("contextmenu.menu_item");
            elem.on("click.menu_item", item_click_handler)
                .on("contextmenu.menu_item", item_click_handler);
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
    var mouse_hover_suppressed = null;

    function add_hover_class(item)
    {
        if (item < 0 || item >= menu.items.length)
            return;
        menu.items[item].elem.addClass("hovered");
    }

    function remove_hover_class(item)
    {
        if (item < 0 || item >= menu.items.length)
            return;
        menu.items[item].elem.removeClass("hovered");
    }

    function mouse_set_hovered(index)
    {
        if (mouse_hover_suppressed)
            return;
        if (index >= 0 &&
            (index < menu.first_part_visible || index > menu.last_part_visible))
        {
            return;
        }
        set_hovered(index, false, true);
    }

    function clear_suppress()
    {
        mouse_hover_suppressed = null;
    }

    function suppress_mouse_hover()
    {
        // ugh -- keep mouseenter from triggering, is there a better way?
        if (mouse_hover_suppressed)
            clearTimeout(mouse_hover_suppressed);
        mouse_hover_suppressed = setTimeout(clear_suppress, 200);
    }

    function set_hovered(index, snap=true, from_mouse=false)
    {
        if (index >= menu.items.length)
            index = Math.max(0, menu.items.length - 1);
        if (index == menu.last_hovered
            && (index < 0 || item_selectable(menu.items[index])))
        {
            // just make sure the hover class is set correctly
            add_hover_class(menu.last_hovered);
            return;
        }
        remove_hover_class(menu.last_hovered);
        if (index < 0 || item_selectable(menu.items[index]))
        {
            menu.last_hovered = index;
            add_hover_class(menu.last_hovered);
            if (menu.last_hovered >= 0 && snap == true)
                snap_in_page(menu.last_hovered);
            comm.send_message("menu_hover",
                {
                    hover: menu.last_hovered,
                    mouse: from_mouse
                });
        }
    }

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
        mouse_hover_suppressed = null;
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

        menu_div.append("<div class='menu_more'></div>");
        update_more();

        if (client.is_watching())
            menu.following_player_scroll = true;

        // suppress mouseenter events on initial display. It might be nice
        // to do this only if the cursor is currently hiddent, but I haven't
        // been able to reliably detect that.
        suppress_mouse_hover();

        ui.show_popup(menu_div, menu["ui-centred"]);
        handle_size_change();

        // if we get focus back after a popup over this one, don't use mouse
        // position at the time to set hover:
        $(ui.top_popup()).on("focusin",
            function (ev)
            {
                suppress_mouse_hover();
            });

        if (menu.flags & enums.menu_flag.START_AT_END)
            scroll_bottom_to_item(menu.items.length - 1, true);
        else if (menu.jump_to)
            scroll_to_item(menu.jump_to, true);
        else if (menu.items.length > 0)
            scroll_to_item(0, true);

        if (menu.last_hovered >= 0)
            add_hover_class(menu.last_hovered);
    }

    function prepare_hoverable_item(item)
    {
        // TODO: mouse movement over a menu item after hover has been moved
        // off it by arrows isn't enough to restore hover; moving the
        // mouse cursor in and out is needed. Worth addressing?
        item.elem.hover(
            function() {
                mouse_set_hovered($(this).index());
            }, function() {
                // XX if this uses mouse_set_hovered, the timing seems
                // to be extremely flaky w.r.t. a new hover.
                if (!(menu.flags & enums.menu_flag.ARROWS_SELECT))
                    set_hovered(-1);
                // otherwise, keep the hover unless mousenter moves it into
                // a new cell
            });
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
            prepare_hoverable_item(item);

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
            // Probably this could should simply replace item? But I'm nervous
            // about changing it from extend...
            $.extend(item, new_item);
            if (new_item.colour === undefined)
                delete item.colour;
            if (new_item.tiles === undefined)
                delete item.tiles;
            if (new_item.hotkeys === undefined)
                delete item.hotkeys;

            set_item_contents(item, item.elem);
        }
    }

    // Scrolling functions

    function next_hoverable_item(reverse, starting_point, start_at_starting_point=false)
    {
        // port of some logic in menu.cc
        var items_tried = 0;
        var max_items;

        menu.flags
        if (menu.flags & enums.menu_flag.WRAP)
            max_items = menu.items.length;
        else if (reverse)
            max_items = menu.last_hovered; // up arrow on no hover does nothing
        else
            max_items = menu.items.length - Math.max(starting_point, 0);

        if (start_at_starting_point && menu.items.length > 0)
            max_items = Math.max(max_items, 1); // consider at least the starting point

        if (max_items <= 0)
            return -1;

        var new_hover = starting_point;
        if (reverse && new_hover < 0)
            new_hover = 0;
        var found = false;
        if (!start_at_starting_point)
            new_hover = new_hover + (reverse ? -1 : 1);

        // find an item that can be selected in the first place
        while (items_tried < max_items)
        {
            items_tried++;
            if (menu.flags & enums.menu_flag.WRAP)
                new_hover = (new_hover + menu.items.length) % menu.items.length;
            new_hover = Math.max(0, Math.min(new_hover, menu.items.length - 1));
            if (item_selectable(menu.items[new_hover])) // TODO: not identical to c++ logic
            {
                found = true;
                break;
            }
            new_hover = new_hover + (reverse ? -1 : 1);
            // new_hover may be invalid if loop exits now, but we don't use it
        }
        if (!found)
            return -1;
        return new_hover;
    }

    function cycle_hover(reverse)
    {
        var next = next_hoverable_item(reverse, menu.last_hovered);
        if (next != -1)
            set_hovered(next);
    }

    // return a positive or negative number indicating hover relative to the
    // current pane. Will return 0 if there is no current hover.
    function get_relative_hover()
    {
        // XX this function has some heuristics to deal with the fact that
        // headers mess up relative hover, and they still aren't perfect
        if (menu.last_hovered < 0 || menu.items.length == 0)
            return 0;

        // use a negative relative hover if we are on the last element, to
        // prevent a common weird interaction with headers. XX should this be
        // generalized?
        if (menu.last_visible == menu.last_hovered)
            return -1;

        var relative_hover = menu.last_hovered - menu.first_visible;

        var headers = 0;

        // factor out headers
        for (var i = 0; i <= relative_hover; i++)
            if (menu.items[menu.first_visible + i].level < 2)
                headers += 1;
        relative_hover = Math.max(0, relative_hover - headers);

        return relative_hover;
    }

    // set the hover relative to the current pane. If the value is negative,
    // index from the end. The value ignores non-hoverable items (headers).
    function set_relative_hover(relative_hover)
    {
        if (menu.items.length == 0)
            return;
        var hover_target = 0;
        if (relative_hover >= 0)
        {
            // skip headers
            for (var i = 0; i <= relative_hover
                        && menu.first_visible + i <= menu.last_visible; i++)
            {
                if (menu.items[menu.first_visible + i].level < 2)
                    relative_hover += 1;
            }

            hover_target = menu.first_visible + relative_hover;
        }
        else // XX headers not handled
            hover_target = menu.last_visible + relative_hover + 1;

        // don't scroll -- lock to whatever is currently visible
        if (hover_target > menu.last_visible)
            hover_target = menu.last_visible;
        else if (hover_target < menu.first_visible)
            hover_target = menu.first_visible;
        set_hovered(next_hoverable_item(false, hover_target, true), true);
    }

    function paging(up)
    {
        if (menu.items.length == 0)
            return;
        if ((menu.flags & enums.menu_flag.ARROWS_SELECT) && menu.last_hovered < 0)
            menu.last_hovered = menu.first_visible;
        var adjust_hover = menu.last_hovered >= 0;
        var relative_hover = get_relative_hover();
        if (up)
        {
            // if we are already at the top, don't scroll, and move any hover
            // to the top
            if (menu.first_visible == 0)
                relative_hover = 0;
            else
                scroll_bottom_to_item(menu.first_visible);
        }
        else
        {
            // if we are already at the end, don't scroll, and move any hover
            // to the end
            if (menu.last_visible == menu.items.length - 1)
                relative_hover = -1;
            else
                scroll_to_item(menu.last_visible);
        }

        if (adjust_hover)
            set_relative_hover(relative_hover)
    }

    function line_down()
    {
        if (menu.length <= 0)
            return;
        var next = menu.first_visible + 1;
        // treat a header and a following item as one item
        if (menu.items[menu.first_visible].level < 2)
            next = next + 1;
        if (next >= menu.items.length)
            next = menu.items.length - 1;
        scroll_to_item(next);
        snap_hover_in_page();
    }

    function line_up()
    {
        var previous = menu.first_visible - 1;
        if (previous < 0)
            previous = 0;
        scroll_to_item(previous);
        snap_hover_in_page();
    }

    function snap_hover_in_page()
    {
        if (menu.last_hovered < 0)
            return;
        else if (menu.last_hovered < menu.first_visible)
            set_hovered(menu.first_visible, false);
        else if (menu.last_hovered > menu.last_visible)
            set_hovered(menu.last_visible, false);
    }

    function snap_in_page(index)
    {
        // simpler than the c++ version! visible indices already calculated
        if (index < 0 || index >= menu.items.length)
            return;
        // the `<=` here is in order to check if a hovered first_visible item
        // is preceded by a header
        if (index <= menu.first_visible)
            scroll_to_item(index);
        else if (index >= menu.last_visible)
            scroll_bottom_to_item(index);
    }

    function scroll_to_item(item_or_index, was_server_initiated)
    {
        var index = (item_or_index.elem ?
                     item_or_index.index : item_or_index);
        if (menu.items.length == 0)
            return;

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        // ensure that an immediately preceding heading is visible
        if (item.index > 0 && menu.items[item.index - 1].level < 2)
            item = menu.items[item.index - 1];
        if (item.index == menu.first_visible)
            return;
        var contents = $(menu.scroller.scrollElement);
        var baseline = contents.children()[0].getBoundingClientRect().top;
        var elem_y = item.elem[0].getBoundingClientRect().top;
        if (menu.flags & enums.menu_flag.ARROWS_SELECT)
            suppress_mouse_hover();

        // allow a bit of extra space for the fade, number may need more tuning
        contents[0].scrollTop = Math.max(0, elem_y - baseline - 18);

        menu.anchor_last = false;
        menu_scroll_handler(was_server_initiated);
    }

    function scroll_bottom_to_item(item_or_index, was_server_initiated)
    {
        var index = (item_or_index.elem ?
                     item_or_index.index : item_or_index);
        if (menu.items.length == 0 || menu.last_visible == index)
            return;

        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $(menu.scroller.scrollElement);
        var baseline = contents.children().offset().top;

        if (menu.flags & enums.menu_flag.ARROWS_SELECT)
            suppress_mouse_hover();

        // allow a bit of extra space for the fade, number may need more tuning
        contents.scrollTop(item.elem.offset().top + item.elem.height() + 24
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
        menu.last_visible = Math.max(0, menu.items.length - 1);
        menu.first_part_visible = -1;
        menu.last_part_visible = -1;

        for (i = 0; i < menu.items.length; i++)
        {
            const item = menu.items[i];
            const item_rect = item.elem[0].getBoundingClientRect();
            // +1 here is for float issues
            if (item_rect.top + 1.0 >= top)
            {
                menu.first_visible = i;
                break;
            }
            else if (item_rect.bottom - top > 10) // XX 10 here is a bit heuristic
                menu.first_part_visible = i;
        }
        for (; i < menu.items.length; i++)
        {
            const item = menu.items[i];
            const item_rect = item.elem[0].getBoundingClientRect();
            // -1 here is for float issues
            if (item_rect.bottom - 1.0 >= bottom)
            {
                // one after the last visible item
                if (bottom - item_rect.top > 10)
                    menu.last_part_visible = i;
                break;
            }
            else
                menu.last_visible = i;
        }
        if (menu.first_part_visible === -1)
            menu.first_part_visible = menu.first_visible;
        if (menu.last_part_visible === -1)
            menu.last_part_visible = menu.last_visible;
        update_more();
    }

    function update_server_scroll()
    {
        if (update_server_scroll_timeout)
        {
            clearTimeout(update_server_scroll_timeout);
            update_server_scroll_timeout = null;
        }

        if (!menu || menu.type == "crt")
            return;

        update_visible_indices();
        comm.send_message("menu_scroll", {
            first: menu.first_visible,
            last: menu.last_visible,
            hover: menu.last_hovered
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

    function title_prompt(data)
    {
        var prompt;

        var restore = function () {
            if (!client.is_watching || !client.is_watching())
                $("#title_prompt").blur();
            menu.elem.find(".menu_title").removeClass("raw_input_mode");
            update_title();
        };

        // manual close from server side: used for raw input mode
        if (data && data.close)
        {
            restore();
            return;
        }

        var title = menu.elem.find(".menu_title");
        if (data && data.raw)
        {
            title.addClass("raw_input_mode")
            return; // everything else is handled manually, including exiting
        }

        if (!data || !data.prompt)
            prompt = "Select what? (regex) ";
        else
            prompt = data.prompt;
        title.html(prompt);
        var input = $("<input id='title_prompt' class='text title_prompt' type='text'>");
        title.append(input);

        // unclear to me exactly why a timeout is needed but it seems to be
        if (!client.is_watching || !client.is_watching())
            setTimeout(function () { input.focus(); }, 50);

        // escape handling: ESC is intercepted in ui.js and triggers blur(), so
        // can't be handled directly here
        input.focusout(function (ev)
            {
                ev.preventDefault();
                comm.send_message("key", { keycode: 27 }); // Send ESC
                return false;
            })

        if (menu.tag == "macro_mapping")
        {
            input.keypress(function (ev) {
                var chr = String.fromCharCode(event.which);
                if (chr == '?')
                {
                    // TODO: a popup from here does not take focus over
                    // the input, which is still receiving keys. But I'm not
                    // sure how to fix...
                    comm.send_message("key", { keycode: ev.which });
                    ev.preventDefault();
                    return false;
                }
            });
        }

        input.keydown(function (ev) {
            if (ev.which == 13)
            {
                // somewhat hacky / oldschool: just send the text + enter.
                // TODO: sync via explicit json
                // first, remove the focusout handler, since this will defocus
                input.off("focusout");
                var enter = String.fromCharCode(13);
                var text = input.val() + enter;
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
        if (menu_stack.length > 1
            && menu_stack[menu_stack.length - 2].last_hovered >= 0)
        {
            suppress_mouse_hover();
        }
        menu_stack.pop();
        ui.hide_popup();
        menu = menu_stack[menu_stack.length - 1];
    }

    function close_all_menus()
    {
        while (menu_stack.length > 0)
            close_menu();
    }

    function update_more()
    {
        if (menu.type != "crt")
        {
            const contents_height = menu.elem.find(".menu_contents_inner").height();
            const contents = $(menu.scroller.scrollElement);
            const avail_height = contents.height();
            var shown_more = contents_height > avail_height
                                                ? menu.more : menu.alt_more;

            const scroll_end = contents_height - avail_height;
            var scroll_percent;
            if (contents[0].scrollTop === 0 || scroll_end <= 0)
                scroll_percent = "top";
            else if (contents[0].scrollTop >= scroll_end)
                scroll_percent = "bot";
            else
            {
                scroll_percent = (contents[0].scrollTop * 100
                                / scroll_end).toFixed(0) + "%";
                if (scroll_percent.length === 2)
                    scroll_percent = " " + scroll_percent;
            }
            shown_more = shown_more.replace(/XXX/, scroll_percent);

            var more = menu.elem.find(".menu_more");
            more.html(util.formatted_string_to_html(shown_more));
            more.css("padding-left", menu_title_indent()+"px");
            more[0].classList.toggle("hidden", shown_more.length === 0);
        }
    }

    function update_menu(data)
    {
        // n.b. this may overwrite hover, but we can't update it yet because
        // menu items won't have been sent. `handle_size_change` will ensure
        // that it does get synced, but are there cases where something more
        // is needed?
        $.extend(menu, data);

        var old_length = menu.items.length;
        menu.items.length = menu.total_items;
        if (menu.total_items < old_length)
        {
            for (var i = old_length; i >= menu.total_items; --i)
                delete menu.items[i];
            // XX I don't really understand what `container` is doing here, but
            // this code is required to shorten a menu
            var container = $("ol");
            container.empty();
            $.each(menu.items, function(i, item) {
                item.elem.data("item", item);
                prepare_hoverable_item(item);
                container.append(item.elem);
            });
        }
        update_title();
        update_more();
    }

    function update_menu_items(data)
    {
        update_item_range(data.chunk_start, data.items);
        handle_size_change();
    }

    function server_menu_scroll(data)
    {
        if (!data.force && !client.is_watching())
            return;

        menu.server_first_visible = data.first;
        if (menu.following_player_scroll || data.force)
        {
            scroll_to_item(data.first, true);
            set_hovered(data.last_hovered);
            update_more();
        }
    }

    comm.register_handlers({
        "menu": open_menu,
        "close_menu": close_menu,
        "close_all_menus": close_all_menus,
        "update_menu": update_menu,
        "update_menu_items": update_menu_items,
        "menu_scroll": server_menu_scroll,
        "title_prompt": title_prompt,
    });

    // Event handlers

    function handle_size_change()
    {
        if (!menu || menu.type == "crt")
            return;

        if (menu.last_hovered > menu.items.length)
            menu.last_hovered = -1; // sanity check
        // ensure hover class is properly set.
        if (menu.last_hovered >= 0 && !item_selectable(menu.items[menu.last_hovered]))
        {
            // a mildly smarter behavior would find the first hoverable position
            // ignoring headers
            cycle_hover(false);
        }
        else
            set_hovered(menu.last_hovered);

        if (menu.anchor_last)
            scroll_bottom_to_item(menu.last_visible, true);
        else if (menu.first_visible)
            scroll_to_item(menu.first_visible, true);
        else
            update_visible_indices();

        update_more();
    }

    function menu_scroll_handler(was_server_initiated)
    {
        // XXX: punt on detecting user-initiated scrolling for now
        if (was_server_initiated === false)
            menu.following_player_scroll = false;

        update_visible_indices();
        schedule_server_scroll();
    }

    function raw_arrow_keys()
    {
        return menu.tag == "macro_mapping"
            && ($(".raw_input_mode").length // raw input mode is up
                || menu.items.length == 0); // new binding input mode
    }

    function menu_keydown_handler(event)
    {
        if (!menu || menu.type === "crt") return;
        // can't check `hidden` class, which is on a 3x containing div
        if (!ui.top_popup() || ui.top_popup().is(":hidden")) return;
        if (ui.top_popup()[0] !== menu.elem[0]) return;

        if (event.altKey || event.ctrlKey) {
            // ???
            if (update_server_scroll_timeout)
                update_server_scroll();
            return;
        }

        // keycodes only: characters go in menu_keypress_handler
        // TODO: this should interface somehow with key_conversion.js
        switch (event.which)
        {
        case 109: // numpad -
            if (menu_has_custom_dash())
                break;
            // otherwise, fall through to pageup:
        case 33: // page up
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            paging(true);
            event.preventDefault();
            return false;
        case 107: // numpad +
        case 34: // page down
            if (raw_arrow_keys())
                break;
            paging();
            event.preventDefault();
            return false;
        case 35: // end
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            scroll_bottom_to_item(menu.total_items - 1);
            if (menu.total_items > 0 && (menu.flags & enums.menu_flag.ARROWS_SELECT))
                set_hovered(next_hoverable_item(true, menu.total_items - 1, true));
            event.preventDefault();
            return false;
        case 36: // home
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            scroll_to_item(0);
            if (menu.flags & enums.menu_flag.ARROWS_SELECT)
                set_hovered(next_hoverable_item(false, 0, true));
            event.preventDefault();
            return false;
        case 38: // up
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            if ((menu.flags & enums.menu_flag.ARROWS_SELECT) && !event.shiftKey)
                cycle_hover(true);
            else
                line_up();
            event.preventDefault();
            return false;
        case 40: // down
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            if ((menu.flags & enums.menu_flag.ARROWS_SELECT) && !event.shiftKey)
                cycle_hover(false);
            else
                line_down();
            event.preventDefault();
            return false;
        case 37: // left
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            if (event.shiftKey)
            {
                line_up();
                event.preventDefault();
                return false;
            }
            break;
        case 39: // right
            if (raw_arrow_keys())
                break; // Treat input as raw, no need to scroll anyway
            if (event.shiftKey)
            {
                line_down();
                event.preventDefault();
                return false;
            }
            break;
        }

        if (update_server_scroll_timeout)
            update_server_scroll();
    }

    function menu_has_custom_dash()
    {
        return menu.tag == "inventory" // drop/pickup use '-' to clear all (TODO: specific tag?)
            || menu.tag == "stash"     // TODO: ??
            || menu.tag == "actions"   // '-' = clear quiver
            || menu.tag == "macros"    // '-' = clear all macros
            || menu.tag == "macro_mapping" // leave available for binding
            || menu.tag == "use_item"; // '-' = unwield
    }

    function menu_keypress_handler(event)
    {
        if (!menu || menu.type === "crt") return;
        if (!ui.top_popup() || ui.top_popup()[0] !== menu.elem[0]) return;
        if (menu.tag == "macro_mapping") return; // Treat input as raw, no need
                                                 // to scroll anyway

        var chr = String.fromCharCode(event.which);

        if (chr == " " && (menu.flags & enums.menu_flag.MULTISELECT)
            && (menu.flags & enums.menu_flag.ARROWS_SELECT))
        {
            chr = ".";
        }

        // characters only: keycodes go in menu_keydown_handler
        switch (chr)
        {
        case "-":
            if (menu_has_custom_dash())
                break;
            // otherwise, fall through to pageup:
        case "<":
            if (menu.tag == "travel")
                break;
        case ";":
            paging(true);
            event.preventDefault();
            return false;

        case ">":
            if (menu.tag == "travel")
                break;
        case "+":
        case " ":
            paging();
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
        if (menu.flags & enums.menu_flag.ARROWS_SELECT)
        {
            set_hovered($(this).index()); // should be unnecesssary?
            // TODO: send a select event, keycode is very ad hoc here
            if (menu.flags & enums.menu_flag.SINGLESELECT)
            {
                if (event.which == 1)
                    comm.send_message("key", { keycode: 13 });
                else if (event.which == 3)
                    comm.send_message("input", { text: '\'' });
            }
            else if (menu.flags & enums.menu_flag.MULTISELECT)
            {
                if (event.which == 1)
                    comm.send_message("key", { keycode: 32 });
                else if (event.which == 3)
                    comm.send_message("input", { text: '\'' });
            }
        }
        // TODO: don't rely on hotkeys here
        else if (item.hotkeys && item.hotkeys.length && event.which == 1)
            comm.send_message("key", { keycode: item.hotkeys[0] });
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
