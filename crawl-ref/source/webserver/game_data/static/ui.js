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
        var $focused = $(document.activeElement);
        if (!$focused.is("[data-sync-id]"))
            return;
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

        // bail if inside a (legacy) input dialog (from line_reader)
        if ($(focused).closest("#input_dialog").length === 1)
            return;
        // or if outside the ui stack (probably the chat dialog)
        if ($(focused).closest("#ui-stack").length !== 1)
            return;

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
        ui_hotkey_handler(ev);
        maybe_prevent_server_from_handling_key(ev);
    }

    function popup_keypress_handler(ev)
    {
        maybe_prevent_server_from_handling_key(ev);
    }

    function show_popup(id, centred, generation_id)
    {
        var $ui_stack = $("#ui-stack");
        var elem = $(id);
        var ephemeral = elem.parent().length === 0;
        elem.detach();

        console.assert(elem.length === 1, "no popup to show");
        var wrapper = wrap_popup(elem, ephemeral);
        wrapper.toggleClass("centred", centred == true);
        wrapper.attr("data-generation-id", generation_id);
        $("#ui-stack").append(wrapper);
        wrapper.stop(true, true).fadeIn(100, function () {
            if (client.is_watching())
                return;
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
                allowOutsideClick: function (ev) {
                    return $(ev.target).closest("#game").length !== 1;
                },
            }).activate();
        }).css("display","");
        if (client.is_watching())
            wrapper.find("input, button").attr("disabled", true);
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
        if (wrapper[0].focus_trap)
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
            }).css("display","");
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

    function utf8_from_key_value(key)
    {
        if (key.length === 1)
            return key.charCodeAt(0);
        else switch (key) {
            case "Tab": return 9;
            case "Enter": return 13;
            case "Escape": return 27;
            default: return 0;
        }
    }

    function key_value_from_utf8(num)
    {
        switch (num) {
            case 9: return "Tab";
            case 13: return "Enter";
            case 27: return "Escape";
            default: return String.fromCharCode(num);
        }
    }

    function ui_hotkey_handler(ev) {
        if (ev.type === "click") {
            var key_value = $(ev.currentTarget).attr("data-hotkey");
            var keycode = utf8_from_key_value(key_value);
            if (keycode)
                comm.send_message("key", { keycode: keycode });
        } else if (ev.type == "keydown") {
            var $elem = $("#ui-stack [data-hotkey=\""+event.key+"\"]");
            if ($elem.length === 1) {
                $elem.click();
                ev.preventDefault();
                ev.stopPropagation();
            }
        }
    }

    function ui_input_handler(ev) {
        // this check is intended to fix a race condition when starting the
        // game: don't send any state info to the server until we have
        // received some. I'm not sure if it is really the right solution...
        if (!has_received_ui_state)
            return;
        var state_msg = {};
        sync_save_state(ev.target, state_msg);
        send_state_sync(ev.target, state_msg);
    }

    function ui_focus_handler(ev) {
        if (ev.type === "focusin") {
            var $elem = $(ev.target);
            if ($elem.closest(".ui-popup[data-generation-id]").length == 0)
                return;
            if (!$elem.is("[data-sync-id]"))
            {
                console.warn("tabbed to non-syncable element: ", $elem[0]);
                return;
            }
            send_state_sync($elem, { has_focus: true });
        } else if (ev.type === "focusout") {
            if (!ev.relatedTarget && top_popup())
                send_state_sync(top_popup(), { has_focus: true });
        }
    }

    var MO = new MutationObserver(function(mutations) {
        mutations.forEach(function(record) {
            var $chat = $(record.target);
            if ($chat.css("display") === "none")
                $chat[0].focus_trap.deactivate();
        });
    });

    function ui_chat_focus_handler(ev) {
        if ($("#chat").hasClass("focus-trap"))
            return;

        var $chat = $("#chat");
        $chat[0].focus_trap = focus_trap($chat[0], {
            escapeDeactivates: true, // n.b. this apparently isn't doing
                                     // anything, over and above escape simply
                                     // hiding the chat div
            onActivate: function () {
                $chat.addClass("focus-trap");
                /* detect chat.js calling hide() via style attribute */
                MO.observe($chat[0], {
                    attributes : true,
                    attributeFilter : ['style'],
                });
            },
            onDeactivate: function () {
                $chat.removeClass("focus-trap");
            },
            returnFocusOnDeactivate: false,
            clickOutsideDeactivates: true, // warning: this only seems to work
                                           // with some slightly weird changes
                                           // to focus-trap.js...
        }).activate();
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

    var has_received_ui_state = false;
    $(document).off("game_init.ui")
        .on("game_init.ui", function () {
        has_received_ui_state = false;
        $(document).off("game_keydown.ui game_keypress.ui")
            .on("game_keydown.ui", ui_key_handler)
            .on("game_keypress.ui", ui_key_handler)
            .off("click.ui", "[data-hotkey]")
            .on("click.ui", "[data-hotkey]", ui_hotkey_handler)
            .off("input.ui focus.ui", "[data-sync-id]")
            .on("input.ui focus.ui", "[data-sync-id]", ui_input_handler)
            .off("focusin.ui focusout.ui")
            .on("focusin.ui focusout.ui", ui_focus_handler)
            .off("focusin.ui", "#chat_input")
            .on("focusin.ui", "#chat_input",  ui_chat_focus_handler);
        $(window).off("resize.ui").on("resize.ui", ui_resize_handler);
    });

    function sync_save_state(elem, state)
    {
        switch (elem.type)
        {
            case "text":
                state.text = elem.value;
                return;
            case "checkbox":
                state.checked = elem.checked;
                return;
        }
    }

    function sync_load_state(elem, state)
    {
        switch (elem.type)
        {
            case "text":
                elem.value = state.text;
                break;
            case "checkbox":
                elem.checked = state.checked;
                break;
            default:
                return;
        }
    }

    var receiving_ui_state = false;

    function send_state_sync(elem, state)
    {
        if (receiving_ui_state)
            return;
        // TODO: remove the existence check once enough time has passed for
        // browser caches to expire
        if (client.in_game && !client.in_game() || client.is_watching())
            return;
        var $target_popup = $(elem).closest("[data-generation-id]");
        if ($target_popup[0] != top_popup().closest("[data-generation-id]")[0])
            return;
        state.generation_id = +$target_popup.attr("data-generation-id");
        state.widget_id = $(elem).attr("data-sync-id") || null;
        comm.send_message("ui_state_sync", state);
    }

    function sync_focus_state(elem)
    {
        if (client.is_watching())
            top_popup().find(".style-focused").removeClass("style-focused");
        if (elem)
        {
            elem.focus();
            if (client.is_watching())
                $(elem).addClass("style-focused");
        }
        else
            document.activeElement.blur();
    }

    comm.register_handlers({
        "ui-state-sync": function (msg) {
            has_received_ui_state = true;
            if (msg.from_webtiles && !client.is_watching())
                return;
            var popup = top_popup();
            if (!popup)
                return;
            var generation_id = +popup.closest("[data-generation-id]")
                    .attr("data-generation-id");
            if (generation_id != msg.generation_id)
                return;
            var elem = msg.widget_id != "" ?
                    popup.find('[data-sync-id='+msg.widget_id+']')[0] : null;
            try {
                receiving_ui_state = true;
                if (msg.has_focus)
                    sync_focus_state(elem);
                else if (elem)
                    sync_load_state(elem, msg);
            } finally {
                receiving_ui_state = false;
            }
        },
    });

    return {
        show_popup: show_popup,
        hide_popup: hide_popup,
        top_popup: top_popup,
        hide_all_popups: hide_all_popups,
        utf8_from_key_value: utf8_from_key_value,
        key_value_from_utf8: key_value_from_utf8,
    };
});
