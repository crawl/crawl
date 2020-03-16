define(["jquery", "comm", "linkify"], function ($, comm, linkify) {
    var new_message_count = 0;
    var spectators = {
            count: 0,
            names: ""
    };
    var history_limit = 25;
    var message_history = [];
    var unsent_message = "";
    var history_pos = -1;
    var chat_hidden = false;

    function update_spectators(data)
    {
        delete data["msg"];
        $.extend(spectators, data);
        $("#spectator_count").html(data.count + " spectators");
        $("#spectator_list").html(data.names);
        $(document).trigger("spectators_changed", [spectators]);
    }

    function receive_message(data)
    {
        var msg = $("<div>").append(data.content);
        var histcon = $('#chat_history_container')[0];
        var atBottom = Math.abs(histcon.scrollHeight - histcon.scrollTop
                                - histcon.clientHeight) < 1.0;
        msg.find(".chat_msg").html(linkify(msg.find(".chat_msg").text()));
        $("#chat_history").append(msg.html() + "<br>");
        if (atBottom)
            histcon.scrollTop = histcon.scrollHeight;
        if ($("#chat_body").css("display") === "none")
        {
            new_message_count++;
            update_message_count();
        }
        $(document).trigger("chat_message", [data.content]);
    }

    function show_in_chat(text)
    {
        receive_message({"content": text});
    }

    function handle_dump(data)
    {
        var msg = $("<span>").addClass("chat_msg chat_automated")
                             .text(data.url + ".txt")
        receive_message({ content: msg[0].outerHTML });
    }

    function update_message_count()
    {
        var press_key = new_message_count > 0 ? " (Press F12)" : "";
        $("#message_count").html(new_message_count + " new messages" + press_key);
        $("#message_count").toggleClass("has_new", new_message_count > 0);
    }

    function chat_message_send(e)
    {
        // The Enter key sends a message.
        if (e.which == 13)
        {
            var content = $("#chat_input").val();
            e.preventDefault();
            e.stopPropagation();
            if (content != "")
            {
                comm.send_message("chat_msg", {
                    text: content
                });
                $("#chat_input").val("");
                $('#chat_history_container').scrollTop($('#chat_history_container')[0].scrollHeight);
                message_history.unshift(content)
                if (message_history.length > history_limit)
                    message_history.length = history_limit;
                history_pos = -1;
                unsent_message = ""
            }
            return false;
        }
        // Up arrow to access message history.
        else if (e.which == 38 && !e.shiftKey)
        {
            e.preventDefault();
            e.stopPropagation();
            var lim = Math.min(message_history.length, history_limit)
            if (message_history.length && history_pos < lim - 1)
            {
                /* Save any unsent input line before reading history so it can
                 * be reloaded after going past the beginning of message
                 * history with down arrow. */
                var cur_line = $("#chat_input").val()
                if (history_pos == -1)
                    unsent_message = cur_line;
                $("#chat_input").val(message_history[++history_pos]);
            }
        }
        // Down arrow to access message history and any unsent message.
        else if (e.which == 40 && !e.shiftKey)
        {
            e.preventDefault();
            e.stopPropagation();
            if (message_history.length && history_pos > -1)
            {
                if (history_pos == 0)
                {
                    message = unsent_message;
                    history_pos--;
                }
                else
                    message = message_history[--history_pos];
                $("#chat_input").val(message);
            }
        }
        // Esc key to return to game.
        else if (e.which == 27)
        {
            e.preventDefault();
            e.stopPropagation();
            toggle();
            $(document.activeElement).blur();
        }
        return true;
    }

    function toggle()
    {
        if ($("#chat_body").css("display") === "none")
        {
            $("#chat_body").slideDown(200);
            new_message_count = 0;
            update_message_count();
            $("#message_count").html("(Esc: back to game)");
            $('#chat_history_container').scrollTop($('#chat_history_container')[0].scrollHeight);
        }
        else
        {
            $("#chat_body").slideUp(200);
            update_message_count();
        }
    }

    function toggle_entire_chat()
    {
        if ($("#chat").css("display") === "none")
        {
            chat_hidden = false;
            // slide looks odd if the chat box is already 1 line
            if ($("#chat_body").css("display") === "none")
                $("#chat").show();
            else
                $("#chat").slideDown(200);
        }
        else
        {
            chat_hidden = true;
            $(document.activeElement).blur();
            // slide looks odd if the chat box is already 1 line
            if ($("#chat_body").css("display") === "none")
                $("#chat").hide();
            else
                $("#chat").slideUp(200);
        }
        $("#chat_hidden").toggle(chat_hidden);
    }

    function super_hide_chat()
    {
        // this hides chat and deletes the + div, so needs a reload or logout
        // to get back the chat window.
        if ($("#chat").css("display") != "none")
            toggle_entire_chat();
        $("#chat_hidden").remove();
    }

    function reset_visibility(in_game)
    {
        if (!in_game)
        {
            $("#chat").hide()
            $("#chat_hidden").hide()
        }
        else
        {
            $("#chat").toggle(!chat_hidden);
            $("#chat_hidden").toggle(chat_hidden);
        }
    }

    function focus()
    {
        if (!$("#chat_hidden").length)
            return;
        if ($("#chat_hidden").css("display") != "none")
            toggle_entire_chat();
        if ($("#chat_body").css("display") === "none")
            toggle();

        $("#chat_history_container").scrollTop($("#chat_history").height());
        $("#chat_input").focus();
    }

    function clear()
    {
        $("#spectator_list").html("&nbsp;");
        $("#chat_history").html("");
        $("#spectator_count").html("0 spectators");
        new_message_count = 0;
        update_message_count();
        $("#chat_body").slideUp(200);
    }

    $(document).ready(function () {
        $("#chat_input").bind("keydown", chat_message_send);
        $("#chat_caption").bind("click", toggle);
        $("#chat_hide_button").bind("click", toggle_entire_chat);
        $("#chat_hidden").bind("click", toggle_entire_chat);
    });

    comm.register_handlers({
        "dump": handle_dump,
        "chat": receive_message,
        "update_spectators": update_spectators,
        "toggle_chat": toggle_entire_chat,
        "super_hide_chat": super_hide_chat
    });

    return {
        spectators: spectators,
        clear: clear,
        focus: focus,
        reset_visibility: reset_visibility,
        show_in_chat: show_in_chat,
    }
});
