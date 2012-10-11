define(["jquery", "comm", "./util"],
function ($, comm, util) {
    var messages = [];
    var old_scroll_top;

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

    function add_message(data)
    {
        var last_message = messages[messages.length-1];
        messages.push(data);
        var msg_elem = $("<div>");
        msg_elem.addClass("game_message");
        var prefix_glyph = $("<span></span>");
        prefix_glyph.addClass("prefix_glyph");
        if (last_message && last_message.turn < data.turn)
            prefix_glyph.html("_");
        else
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
        $("#messages").append(msg_elem);
        $("#messages_container")
            .stop(true, false)
            .animate({
            scrollTop: $("#messages").height()
        }, "fast");
    }

    function rollback(count)
    {
        messages = messages.slice(0, -count);
        $("#messages .game_message").slice(-count).remove();
    }

    function handle_messages(msg)
    {
        if (msg.rollback)
            rollback(msg.rollback);
        if (msg.old_msgs)
            rollback(msg.old_msgs);
        for (var i = 0; i < msg.data.length; ++i)
        {
            add_message(msg.data[i]);
        }
    }

    function get_line(msg)
    {
        var prompt = $("#messages .game_message").last();
        var input = $("<input class='text' type='text'>");
        prompt.append(input);

        input.focus();

        function restore()
        {
            input.blur();
            input.remove();
        }

        input.keydown(function (ev) {
            if (ev.which == 27)
            {
                ev.preventDefault();
                comm.send_message("key", { keycode: 27 }); // Send ESC
                return false;
            }
            else if (ev.which == 13)
            {
                var enter = String.fromCharCode(13);
                var text = input.val() + enter;
                comm.send_message("input", { text: text});
                ev.preventDefault();
                return false;
            }
        });
    }

    function abort_get_line()
    {
        var input = $("#messages .game_message input");
        input.blur();
        input.remove();
    }

    function hide_more()
    {
        $("#more").hide();
    }

    function more(full, user)
    {
        $("#more").show();
    }

    function handle_more(data)
    {
        more(data.full, data.user);
    }

    comm.register_handlers({
        "msgs": handle_messages,
        "get_line": get_line,
        "abort_get_line": abort_get_line,
        "more": handle_more,
        "hide_more": hide_more
    });

    $(document).off("game_init.messages")
        .on("game_init.messages", function () {
            messages = [];
        });

    return {
        hide: hide,
        show: show
    };
});
