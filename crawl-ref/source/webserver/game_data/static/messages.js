define(["jquery", "comm", "client", "./util", "./options"],
function ($, comm, client, util, options) {
    "use strict";

    var HISTORY_SIZE = 10;

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

    /**
    * Remove all message elements from the player messages window save for the
    * last 15.

    * This is necessary to prevent <div> elements from messages no longer in
    * view from pilling up over longer WebTiles session and thus slowing down
    * the browser.
    */
    function remove_old_messages()
    {
        var all_messages = $("#messages .game_message");
        if (all_messages.length > 15)
        {
            var messages_to_remove = all_messages.slice(0, -15);
            messages_to_remove.remove();
        }
    }

    function add_message(data)
    {
        remove_old_messages();
        var msg_elem = $("<div>");
        $("#messages").append(msg_elem);
        msg_elem.addClass("game_message");
        var prefix_glyph = $("<span></span>");
        prefix_glyph.addClass("prefix_glyph");
        prefix_glyph.html("&nbsp;");
        msg_elem.html(prefix_glyph);
        msg_elem.append(util.formatted_string_to_html(data.text));
        $("#messages_container").scrollTop($("#messages").height());
    }

    function rollback(count)
    {
        $("#messages .game_message").slice(-count).remove();
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
            input.attr("maxlength", msg.maxlen);
        if ("size" in msg)
            input.attr("size", msg.size);
        if ("prefill" in msg)
            input.val(msg.prefill);
        if ("historyId" in msg)
        {
            if (!histories[msg.historyId])
                histories[msg.historyId] = [];
            history = histories[msg.historyId];
            historyPosition = history.length;
        }

        prompt.append(input);
        if (input[0].setSelectionRange)
            input[0].setSelectionRange(input.val().length, input.val().length);
        input.focus();

        function send_input_line(finalChar) {
            var input = $("#messages .game_message input");
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
            var input = $("#messages .game_message input");
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
                if (input[0].setSelectionRange)
                    input[0].setSelectionRange(input.val().length, input.val().length);
                ev.preventDefault();
                return false;
            }
        });
        input.keypress(function (ev) {
            var input = $("#messages .game_message input");
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

    options.add_listener(function ()
    {
        if (options.get("tile_font_msg_size") === 0)
            $("#message_pane").css("font-size", "");
        else
        {
            $("#message_pane").css("font-size",
                options.get("tile_font_msg_size") + "px");
        }

        var family = options.get("tile_font_msg_family");
        if (family !== "" && family !== "monospace")
        {
            family += ", monospace";
            $("#message_pane").css("font-family", family);
        }
    });

    comm.register_handlers({
        "msgs": handle_messages,
        "text_cursor": text_cursor,
        "get_line": get_line,
        "abort_get_line": abort_get_line
    });

    $(document).off("game_init.messages")
        .on("game_init.messages", function () {
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
