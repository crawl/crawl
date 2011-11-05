define(["jquery", "comm", "client", "./enums"], function ($, comm, client, enums) {
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

    function set_item_contents(item, elem)
    {
        elem.html(formatted_string_to_html(item_text(item)));
        var col = item_colour(item);
        // Remove old color
        elem.attr("class", function (i, cls) {
            return cls && cls.replace(/\bfg\S+/g, "");
        });
        elem.addClass("fg" + col);
    }

    var cols = {
        "black": 0,
        "blue": 1,
        "green": 2,
        "cyan": 3,
        "red": 4,
        "magenta": 5,
        "brown": 6,
        "lightgrey": 7,
        "lightgray": 7,
        "darkgrey": 8,
        "darkgray": 8,
        "lightblue": 9,
        "lightgreen": 10,
        "lightcyan": 11,
        "lightred": 12,
        "lightmagenta": 13,
        "yellow": 14,
        "white": 15
    };

    function formatted_string_to_html(str)
    {
        var other_open = false;
        return str.replace(/<(\/?[a-z]+)>/ig, function (str, p1) {
            var closing = false;
            if (p1.match(/^\//))
            {
                p1 = p1.substr(1);
                closing = true;
            }
            if (p1 in cols)
            {
                if (closing)
                    return "</span>";
                else
                {
                    var text = "<span class='fg" + cols[p1] + "'>";
                    if (other_open)
                        text = "</span>" + text;
                    other_open = true;
                    return text;
                }
            }
            else
                return str;
        }).replace(/<</g, "<");
    }

    var menu_stack = [];

    function get_menu()
    {
        if (menu_stack.length == 0) return null;
        return menu_stack[menu_stack.length-1];
    }

    function display_menu()
    {
        client.set_layer("normal");

        var menu = get_menu();
        if (menu.elem)
        {
            $("#menu").html(menu.elem);
            return;
        }
        var menu_div = $("<div>");
        menu_div.addClass("menu_" + menu.tag);
        menu.elem = menu_div;

        $("#menu").html(menu_div);

        if (menu.title && menu.title.text)
        {
            menu_div.prepend("<div id='menu_title'>");
            update_title();
        }

        var items = $("<div id='menu_contents'>");
        items.css({
            "max-height": $(window).height() - 100
        });
        var items_inner = $("<div id='menu_contents_inner'>");
        items.append(items_inner);
        var container = $("<ol>");
        items_inner.append(container);
        for (var i = 0; i < menu.items.length; ++i)
        {
            var item = menu.items[i];
            if (typeof item === "string")
            {
                item = { level: 2, text: item };
                menu.items[i] = item;
            }
            var elem = $("<li>");
            elem.data("item", item);
            elem.addClass("level" + item.level);
            item.elem = elem;
            set_item_contents(item, elem);

            container.append(elem);
        }

        items_inner.append("<br>");

        menu_div.append(items);

        menu_div.append("<div id='menu_more'>" + formatted_string_to_html(menu.more)
                        + "</div>");

        items.scroll(menu_scroll_handler);

        client.show_dialog("#menu");

        update_visible_indices();
        if (menu.flags & enums.menu_flag.START_AT_END)
        {
            scroll_to_end();
        }
        // Hide -more- if at the bottom
        menu_scroll_handler();
    }

    function open_menu(data)
    {
        log(data);
        menu_stack.push(data);

        display_menu();
    }

    function close_menu()
    {
        menu_stack.pop();

        log("close_menu");

        if (menu_stack.length == 0)
            client.hide_dialog();
        else
            display_menu();
    }

    function update_menu(data)
    {
        log(data);

        var menu = get_menu();
        $.extend(menu, data);

        update_title();

        client.center_element($("#menu"));
    }

    function update_menu_item(data)
    {
        log(data);

        var menu = get_menu(), item = menu.items[data.i];
        if (!item) return;
        $.extend(item, data);
        if (data.colour == undefined)
            delete item.colour;

        set_item_contents(item, item.elem);

        handle_size_change();
    }

    function update_title()
    {
        var menu = get_menu();
        set_item_contents(menu.title, $("#menu_title"));
        if (menu.suffix)
        {
            $("#menu_title").append(" " + menu.suffix);
        }
    }

    function init_menus(data)
    {
        if (menu_stack.length > 0) return;

        menu_stack = data.menus;
        if (get_menu())
            display_menu();
    }

    function scroll_to_end()
    {
        var menu = get_menu();
        scroll_bottom_to_item(menu.items.length - 1);
    }

    function page_down()
    {
        var menu = get_menu();
        var next = menu.last_visible + 1;
        if (next >= menu.items.length)
            next = menu.items.length - 1;
        scroll_to_item(next);
    }

    function page_up()
    {
        var menu = get_menu();
        var previous = menu.first_visible - 1;
        if (previous < 0)
            previous = 0;
        scroll_bottom_to_item(previous);
    }

    function line_down()
    {
        var menu = get_menu();
        var next = menu.first_visible + 1;
        if (next >= menu.items.length)
            next = menu.items.length - 1;
        scroll_to_item(next);
    }

    function line_up()
    {
        var menu = get_menu();
        var previous = menu.first_visible - 1;
        if (previous < 0)
            previous = 0;
        scroll_to_item(previous);
    }

    function scroll_to_item(item_or_index)
    {
        var menu = get_menu();
        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $("#menu_contents");
        var baseline = contents.children().offset().top;

        contents.scrollTop(item.elem.offset().top - baseline);
    }

    function scroll_bottom_to_item(item_or_index)
    {
        var menu = get_menu();
        var item = (item_or_index.elem ?
                    item_or_index : menu.items[item_or_index]);
        var contents = $("#menu_contents");
        var baseline = contents.children().offset().top;

        contents.scrollTop(item.elem.offset().top + item.elem.height()
                           - baseline - contents.innerHeight());
    }

    function update_visible_indices()
    {
        var menu = get_menu();
        var contents = $("#menu_contents");
        var top = contents.offset().top;
        var bottom = top + contents.innerHeight();
        menu.first_visible = null;
        for (var i = 0; i < menu.items.length; ++i)
        {
            var item = menu.items[i];
            var item_top = item.elem.offset().top;
            if (item_top >= top && menu.first_visible === null)
            {
                menu.first_visible = i;
            }
            if (item_top + item.elem.height() >
                bottom)
            {
                menu.last_visible = i - 1;
                return;
            }
        }
        menu.first_visible = menu.first_visible || 0;
        menu.last_visible = menu.items.length - 1;
    }

    function handle_size_change()
    {
        var menu = get_menu();
        if (!menu) return;

        var items = $("#menu_contents");
        items.css({
            "max-height": $(window).height() - 100
        });
        client.center_element($("#menu"));
        scroll_to_item(menu.first_visible);
        menu_scroll_handler();
    }

    function menu_scroll_handler()
    {
        var menu = get_menu(), contents = $("#menu_contents");
        update_visible_indices();
        if (menu.last_visible >= menu.items.length - 1)
        {
            if (!(menu.flags & enums.menu_flag.ALWAYS_SHOW_MORE))
            {
                $("#menu_more").css("visibility", "hidden");
            }
        }
        else
        {
            $("#menu_more").css("visibility", "visible");
        }
    }

    function menu_keydown_handler(event)
    {
        if (event.altKey || event.ctrlKey || event.shiftkey)
            return;

        if (!get_menu() || get_menu().type === "crt") return;

        switch (event.which)
        {
        case 38: // up
            line_up();
            return false;
        case 40: // down
            line_down();
            event.preventDefault();
            return false;
        }
    }

    function menu_keypress_handler(event)
    {
        if (!get_menu() || get_menu().type === "crt") return;

        var chr = String.fromCharCode(event.which);
        switch (chr)
        {
        case "<":
        case "-":
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
    }

    $(document).off("game_init.menu");
    $(document).on("game_init.menu", function () {
        menu_stack = [];
        $(window).off("resize.menu");
        $(document).off("game_keydown.menu");
        $(document).off("game_keypress.menu");
        $(window).on("resize.menu", handle_size_change);
        $(document).on("game_keydown.menu", menu_keydown_handler);
        $(document).on("game_keypress.menu", menu_keypress_handler);
    });

    comm.register_handlers({
        "menu": open_menu,
        "close_menu": close_menu,
        "update_menu": update_menu,
        "update_menu_item": update_menu_item,
        "init_menus": init_menus
    });
});
