define(["jquery", "comm", "client", "./options", "./focus-trap"],
function ($, comm, client, options, focus_trap) {
    "use strict";

    function wrap_popup(elem, ephemeral)
    {
        var wrapper = $(".templates > .ui-popup").clone();
        wrapper.data("ephemeral", ephemeral);
        wrapper.find(".ui-popup-inner").append(elem.removeClass("hidden"));
        return wrapper;
    }

    function unwrap_popup(wrapper)
    {
        console.assert(wrapper.hasClass("ui-popup"), "trying to unwrap something that hasn't been wrapped");
        return wrapper.find(".ui-popup-inner").children();
    }

    function maybe_prevent_server_from_handling_key(ev)
    {
        if (document.activeElement == document.body)
            return;
        var $focused = $(document.activeElement);
        if ($focused.is("input[type=text]"))
            ev.stopPropagation();
        // stop server handling checkbox/button space/enter key events
        var checkbox = $focused.is("input[type=checkbox]");
        var button = $focused.is("input[type=button], button");
        var enter = ev.which == 13;
        var space = ev.which == 32;
        if ((checkbox || button) && (enter || space))
            ev.stopPropagation();
    }

    function popup_keydown_handler(ev)
    {
        var wrapper = $("#ui-stack").children().last();
        var focused = document.activeElement != document.body ?
                document.activeElement : null;

        if (ev.key == "Escape" && focused)
        {
            document.activeElement.blur();
            ev.stopPropagation();
        }

        if (ev.key == "Tab")
        {
            if (focused)
            {
                ev.stopPropagation();
                return;
            }

            var focusable = wrapper[0].querySelectorAll(
                'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
            if (focusable.length == 0)
                return;
            var first = focusable[0];
            var last = focusable[focusable.length-1];
            (ev.shiftKey ? last : first).focus();
            ev.preventDefault();
            ev.stopPropagation();
        }

        maybe_prevent_server_from_handling_key(ev);
    }

    function popup_keypress_handler(ev)
    {
        maybe_prevent_server_from_handling_key(ev);
    }

    function show_popup(id, centred)
    {
        var $ui_stack = $("#ui-stack");
        var elem = $(id);
        var ephemeral = elem.parent().length === 0;
        elem.detach();

        console.assert(elem.length === 1, "no popup to show");
        var wrapper = wrap_popup(elem, ephemeral);
        wrapper.toggleClass("centred", centred == true);
        $("#ui-stack").append(wrapper);
        wrapper.stop(true, true).fadeIn(100, function () {
            wrapper[0].focus_trap = focus_trap(elem[0], {
                escapeDeactivates: false,
                fallbackFocus: document.body,
                onActivate: function () {
                    if ($("#ui-stack").children().length == 1) {
                        document.addEventListener("keydown",
                            popup_keydown_handler, true);
                        document.addEventListener("keypress",
                            popup_keypress_handler, true);
                    }
                },
                onDeactivate: function () {
                    if ($("#ui-stack").children().length == 1) {
                        document.removeEventListener("keydown",
                            popup_keydown_handler, true);
                        document.removeEventListener("keypress",
                            popup_keypress_handler, true);
                    }
                },
            }).activate();
        });
        if (elem.find(".paneset").length > 0)
            ui_resize_handler();
    }

    function hide_popup(show_below)
    {
        var $ui_stack = $("#ui-stack");
        var wrapper = $ui_stack.children().last();
        console.assert(wrapper.length === 1, "no popup to hide");

        var elem = unwrap_popup(wrapper).blur();
        if (!wrapper.data("ephemeral"))
            elem.detach().addClass("hidden").appendTo("body");
        wrapper[0].focus_trap.deactivate();
        wrapper.remove();

        if (show_below === false)
            return;

        // Now show revealed menu
        var wrapper = $ui_stack.children().last();
        if (wrapper.length > 0)
        {
            elem = unwrap_popup(wrapper);
            wrapper.stop(true, true).fadeIn(100, function () {
                elem.focus();
            });
        }
    }

    function top_popup()
    {
        var $popup = $("#ui-stack").children().last();
        if ($popup.length === 0)
            return;
        return $popup.find(".ui-popup-inner").children().eq(0);
    }

    function hide_all_popups()
    {
        var $ui_stack = $("#ui-stack");
        while ($ui_stack.children().length > 0)
            hide_popup(false);
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

        if (new_ev.isDefaultPrevented())
            ev.preventDefault();
        if (new_ev.isImmediatePropagationStopped())
            ev.stopImmediatePropagation();
    }

    function ui_resize_handler (ev)
    {
        if ($.browser.webkit)
        {
            $("#ui-stack .paneset").each(function (i, el) {
                $(el).children(".pane").css("height", "");
                var height = $(el).outerHeight() + "px";
                $(el).children(".pane").css("height", height);
            });

            $("#ui-stack [data-simplebar]").each(function (i, el) {
                $(el).data("scroller").recalculateImmediate();
            });
        }
    }

    options.add_listener(function ()
    {
        var size = options.get("tile_font_crt_size");
        $("#ui-stack").css("font-size", size === 0 ? "" : (size + "px"));

        var family = options.get("tile_font_crt_family");
        if (family !== "" && family !== "monospace")
        {
            family += ", monospace";
            $("#ui-stack").css("font-family", family);
        }

        $("#ui-stack").attr('data-display-mode',
                options.get("tile_display_mode"));
    });

    $(document).off("game_init.ui")
        .on("game_init.ui", function () {
        $(document).off("game_keydown.ui game_keypress.ui")
            .on("game_keydown.ui", ui_key_handler)
            .on("game_keypress.ui", ui_key_handler);
        $(window).off("resize.ui").on("resize.ui", ui_resize_handler);
    });

    return {
        show_popup: show_popup,
        hide_popup: hide_popup,
        top_popup: top_popup,
        hide_all_popups: hide_all_popups,
    };
});
