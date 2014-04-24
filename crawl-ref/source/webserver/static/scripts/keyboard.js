define(["jquery", "comm", "key_conversion", "client", "pubsub"],
function ($, comm, key_conversion, client, pubsub) {
    function text_input_focused()
    {
        var tn = document.activeElement.tagName;
        var typ = document.activeElement.type;
        if (tn === "TEXTAREA") return true;
        if (tn === "INPUT" &&
            (typ === "text" ||
             typ === "password" ||
             typ === "email"))
        {
            return true;
        }
        return false;
    }

    function in_game()
    {
        return client.state === "play" ||
            client.state === "watch";
    }

    function watching()
    {
        return client.state === "watch";
    }

    function handle_keypress(e)
    {
        if (!in_game()) return;
        if (text_input_focused()) return;

        // Fix for FF < 25:
        // https://developer.mozilla.org/en-US/docs/Web/Reference/Events/keydown#preventDefault%28%29_of_keydown_event
        if (e.isDefaultPrevented()) return;

        if (e.ctrlKey || e.altKey)
        {
            // allow AltGr keys on various non-english keyboard layouts, not
            // needed for Mozilla where neither ctrlKey or altKey is set
            if ($.browser.mozilla || !e.ctrlKey || !e.altKey)
                return;
        }

        if ((e.which == 0) ||
            (e.which == 8) || /* Backspace gets a keypress in FF, but not Chrome
                                 so we handle it in keydown */
            (e.which == 9))   /* Tab gives a keypress in Opera even when it is
                                 suppressed in keydown */
            return;

        // Give the game a chance to handle the key
        if (!retrigger_event(e, "game_keypress"))
            return;

        if (watching())
            return;

        e.preventDefault();

        var s = String.fromCharCode(e.which);
        if (s == "{")
        {
            send_bytes(["{".charCodeAt(0)]);
        }
        else
            comm.send_message("input", { text: s });
    }

    function send_keycode(code)
    {
        comm.send_raw('{"msg":"key","keycode":' + code + '}');
    }

    function send_bytes(bytes)
    {
        s = '{"msg":"input","data":[';
        $.each(bytes, function (i, code) {
            if (i == 0)
                s += code;
            else
                s += "," + code;
        });
        s += "]}";
        comm.send_raw(s);
    }

    function retrigger_event(ev, new_type)
    {
        var new_ev = $.extend({}, ev);
        new_ev.type = new_type;
        $(new_ev.target).trigger(new_ev);
        if (new_ev.isDefaultPrevented())
            ev.preventDefault();
        if (new_ev.isPropagationStopped())
        {
            ev.stopImmediatePropagation();
            return false;
        }
        else
            return true;
    }

    function handle_keydown(e)
    {
        if (!in_game()) return;
        if (text_input_focused()) return;

        // Give the game a chance to handle the key
        if (!retrigger_event(e, "game_keydown"))
            return;

        if (watching())
        {
            if (!e.ctrlKey && !e.shiftKey && !e.altKey)
            {
                if (e.which == 27 || String.fromCharCode(e.which) == "Q")
                {
                    e.preventDefault();
                    document.location = "/";
                }
                else if (e.which == 123)
                {
                    e.preventDefault();
                    pubsub.publish("chat_hotkey");
                }
            }
            return;
        }

        if (e.ctrlKey && !e.shiftKey && !e.altKey)
        {
            if (e.which in key_conversion.ctrl)
            {
                e.preventDefault();
                send_keycode(key_conversion.ctrl[e.which]);
            }
            else if ($.inArray(String.fromCharCode(e.which),
                               key_conversion.captured_control_keys) != -1)
            {
                e.preventDefault();
                var code = e.which - "A".charCodeAt(0) + 1; // Compare the CONTROL macro in defines.h
                send_keycode(code);
            }
        }
        else if (!e.ctrlKey && e.shiftKey && !e.altKey)
        {
            if (e.which in key_conversion.shift)
            {
                e.preventDefault();
                send_keycode(key_conversion.shift[e.which]);
            }
        }
        else if (!e.ctrlKey && !e.shiftKey && e.altKey)
        {
            if (e.which < 32) return;

            e.preventDefault();
            send_bytes([27, e.which]);
        }
        else if (!e.ctrlKey && !e.shiftKey && !e.altKey)
        {
            if (e.which == 123)
            {
                e.preventDefault();
                pubsub.publish("chat_hotkey");
            }
            else if (e.which in key_conversion.simple)
            {
                e.preventDefault();
                send_keycode(key_conversion.simple[e.which]);
            }
        }
    }

    $(document).on("keypress.client", handle_keypress);
    $(document).on("keydown.client", handle_keydown);

    return {
        handle_keypress: handle_keypress,
        handle_keydown: handle_keydown
    };
});
