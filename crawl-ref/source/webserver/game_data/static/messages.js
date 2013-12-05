define(["jquery", "comm", "client", "./util", "./settings"],
function ($, comm, client, util, settings) {
    var HISTORY_SIZE = 10;

    var messages = [];
    var more = false;
    var old_scroll_top;
    var histories = {};

    function hide()
    {
        old_scroll_top = $("#messages_container").scrollTop();
        $("#messages").hide();
    }

    function show()
    {
        $("#messages").show();
        $("#messages_container").scrollTop(old_scroll_top);
    }

    var prefix_glyph_classes = "turn_marker command_marker";
    function set_last_prefix_glyph(text, classes)
    {
        var last_msg_elem = $("#messages .game_message").last();
        var prefix_glyph = last_msg_elem.find(".prefix_glyph");
        prefix_glyph.text(text);
        prefix_glyph.addClass(classes);
    }

    function new_command(new_turn)
    {
        $("#more").hide();
        if (new_turn)
            set_last_prefix_glyph("_", "turn_marker");
        else
            set_last_prefix_glyph("_", "command_marker");
    }

    function add_message(data)
    {
        var last_message = messages[messages.length-1];
        messages.push(data);
        var msg_elem;
        var reusable_msg_elems = $("#messages .game_message.cleared");
        if (reusable_msg_elems.length > 0)
        {
            msg_elem = reusable_msg_elems.first();
            msg_elem.removeClass("cleared");
        }
        else
        {
            msg_elem = $("<div>");
            $("#messages").append(msg_elem);
        }
        msg_elem.addClass("game_message");
        var prefix_glyph = $("<span></span>");
        prefix_glyph.addClass("prefix_glyph");
        prefix_glyph.html("&nbsp;");
        msg_elem.html(prefix_glyph);
        msg_elem.append(util.formatted_string_to_html(data.text));
        if (data.repeats > 1)
        {
            var repeats = $("<span>");
            repeats.addClass("repeats");
            repeats.text("(" + data.repeats + "x)");
            msg_elem.append(" ");
            msg_elem.append(repeats);
        }
        if (settings.get("animations"))
        {
            $("#messages_container")
                .stop(true, false)
                .animate({
                    scrollTop: $("#messages").height()
                }, 100);
        }
        else
        {
            $("#messages_container").scrollTop($("#messages").height());
        }
    }

    function rollback(count)
    {
        messages = messages.slice(0, -count);
        $("#messages .game_message").not(".cleared").slice(-count)
            .addClass("cleared").html("&nbsp;");
    }

    function handle_messages(msg)
    {
        if (msg.rollback)
            rollback(msg.rollback);
        if (msg.old_msgs)
            rollback(msg.old_msgs);
        if ("more" in msg)
            more = msg.more;
        $("#more").toggle(more);
        if (msg.messages)
        {
            for (var i = 0; i < msg.messages.length; ++i)
            {
                add_message(msg.messages[i]);
            }
        }
        var cursor = $("#text_cursor");
        if (cursor.length > 0)
            cursor.appendTo($("#messages .game_message").last());
    }

    function get_line(msg)
    {
        if (client.is_watching != null && client.is_watching())
            return;

        assert(msg.tag !== "repeat" || !msg.prefill);

        $("#text_cursor").remove();

        var prompt = $("#messages .game_message").last();
        var input = $("<input class='text' type='text'>");
        var history;
        var historyPosition;

        if ("maxlen" in msg)
            input.attr("maxlength", msg.maxlen)
        if ("size" in msg)
            input.attr("size", msg.size)
        if ("prefill" in msg)
            input.val(msg.prefill)
        if ("historyId" in msg)
        {
            if (!histories[msg.historyId])
                histories[msg.historyId] = [];
            history = histories[msg.historyId];
            historyPosition = history.length;
        }

        prompt.append(input);
        input.focus();

        function send_input_line(finalChar) {
            if (history && input.val().length > 0)
            {
                if (history.indexOf(input.val()) != -1)
                    history.splice(history.indexOf(input.val()), 1);
                history.push(input.val());
                if (history.length > HISTORY_SIZE)
                    history.shift();
            }

            var text = input.val() + String.fromCharCode(finalChar);
            // ctrl-u to wipe any pre-fill
            if (msg.tag !== "repeat")
                comm.send_message("key", { keycode: 21 });
            comm.send_message("input", { text: text });
            abort_get_line();
        }

        input.keydown(function (ev) {
            if (ev.which == 27)
            {
                ev.preventDefault();
                comm.send_message("key", { keycode: 27 }); // Send ESC
                return false;
            }
            else if (ev.which == 13) // enter
            {
                send_input_line(13);
                ev.preventDefault();
                return false;
            }
            else if (history && history.length > 0 && (ev.which == 38 || ev.which == 40))
            {
                historyPosition += ev.which == 38 ? -1 : 1;
                if (historyPosition < 0)
                    historyPosition = history.length - 1;
                if (historyPosition >= history.length)
                    historyPosition = 0;
                input.val(history[historyPosition]);
            }
        });
        input.keypress(function (ev) {
            if (msg.tag == "stash_search")
            {
                if (String.fromCharCode(ev.which) == "?" && input.val().length === 0)
                {
                    ev.preventDefault();
                    comm.send_message("key", { keycode: ev.which });
                    return false;
                }
            }
            else if (msg.tag == "repeat")
            {
                var ch = String.fromCharCode(ev.which);
                if (ch != "" && !/^\d$/.test(ch))
                {
                    send_input_line(ev.which);
                    ev.preventDefault();
                    return false;
                }
            }
            else if (msg.tag == "travel_depth")
            {
                var ch = String.fromCharCode(ev.which);
                if ("<>?$^-p".indexOf(ch) != -1)
                {
                    send_input_line(ev.which);
                    ev.preventDefault();
                    return false;
                }
            }
        });
    }

    function messages_key_handler()
    {
        var input = $("#messages .game_message input");

        if (!input.is(":visible"))
            return true;

        input.focus();
        return false;
    }

    function abort_get_line()
    {
        var input = $("#messages .game_message input");
        input.blur();
        input.remove();
    }

    function text_cursor(data)
    {
        if (data.enabled)
        {
            if ($("#text_cursor").length > 0)
                return;
            if ($("#messages .game_message input").length > 0)
                return;
            var cursor = $("<span id='text_cursor'>_</span>");
            $("#messages .game_message").last().append(cursor);
        }
        else
        {
            $("#text_cursor").remove();
        }
    }

    comm.register_handlers({
        "msgs": handle_messages,
        "text_cursor": text_cursor,
        "get_line": get_line,
        "abort_get_line": abort_get_line
    });

    $(document).off("game_init.messages")
        .on("game_init.messages", function () {
            messages = [];
            more = false;
            $(document).off("game_keydown.messages game_keypress.messages")
                .on("game_keydown.messages", messages_key_handler)
                .on("game_keypress.messages", messages_key_handler);
        });

    return {
        hide: hide,
        show: show,
        new_command: new_command
    };
});
