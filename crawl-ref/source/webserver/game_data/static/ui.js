define(["jquery", "comm", "client", "./options"],
function ($, comm, client, options) {
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

    function show_popup(id)
    {
        var $ui_stack = $("#ui-stack");
        var elem = $(id);
        var ephemeral = elem.parent().length === 0;
        elem.detach();

        console.assert(elem.length === 1, "no popup to show");
        var wrapper = wrap_popup(elem, ephemeral);
        $("#ui-stack").append(wrapper);
        wrapper.stop(true, true).fadeIn(100, function () {
            elem.focus();
        });
    }

    function hide_popup(show_below)
    {
        var $ui_stack = $("#ui-stack");
        var wrapper = $ui_stack.children().last();
        console.assert(wrapper.length === 1, "no popup to hide");

        var elem = unwrap_popup(wrapper).blur();
        if (!wrapper.data("ephemeral"))
            elem.detach().addClass("hidden").appendTo("body");
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

        if (new_ev.default_prevented)
            ev.preventDefault();
        if (new_ev.propagation_stopped)
            ev.stopImmediatePropagation();
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
    });


    $(document).off("game_init.ui")
        .on("game_init.ui", function () {
        $(document).off("game_keydown.ui game_keypress.ui")
            .on("game_keydown.ui", ui_key_handler)
            .on("game_keypress.ui", ui_key_handler);
    });

    return {
        show_popup: show_popup,
        hide_popup: hide_popup,
        top_popup: top_popup,
        hide_all_popups: hide_all_popups,
    };
});
