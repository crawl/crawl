define(["jquery", "comm", "client"],
function ($, comm, client) {
    "use strict";

    var $ui_stack = $("#ui-stack");

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
        var wrapper = $ui_stack.children().last();
        if (wrapper.length !== 1)
            return;

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

    function hide_all_popups()
    {
        while ($ui_stack.children().length > 0)
            hide_popup(false);
    }

    return {
        show_popup: show_popup,
        hide_popup: hide_popup,
        hide_all_popups: hide_all_popups,
    };
});
