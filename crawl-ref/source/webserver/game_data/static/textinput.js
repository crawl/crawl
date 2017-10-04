define(["jquery", "comm", "client", "./enums", "./util", "./options"],
function ($, comm, client, enums, util, options) {
    "use strict";

    var HISTORY_SIZE = 10;
    var input_data = null;
    var histories = {};

    // Helpers

    function input_cleanup()
    {
        input_data = null;
    }

    function find_input()
    {
        assert(input_data);
        if (input_data.type == "messages")
            return $("#messages .game_message input");
        else  //"generic"
            return $("#input_dialog input");
    }

    function display_input()
    {
        var input = $("<input class='text' type='text'>");
        var prompt = input_data.prompt; // may be null

        var history;
        var historyPosition;

        if ("maxlen" in input_data)
            input.attr("maxlength", input_data.maxlen);
        if ("size" in input_data)
            input.attr("size", input_data.size);
        if ("prefill" in input_data)
            input.val(input_data.prefill.trim());
        if ("historyId" in input_data)
        {
            if (!histories[input_data.historyId])
                histories[input_data.historyId] = [];
            history = histories[input_data.historyId];
            historyPosition = history.length;
        }

        if (input[0].setSelectionRange)
            input[0].setSelectionRange(input.val().length, input.val().length);
        input_data.input = input;

        if (input_data.type == "messages")
        {
            $("#text_cursor").remove();
            prompt = $("#messages .game_message").last();
            prompt.append(input);
        }
        else // "generic"
        {
            var input_div = $("<div>");
            input_div.addClass("input_" + input_data.tag);
            $("#input_dialog").html(input_div);

            input_div.attr("id", "input_text");
            if (!prompt)
                prompt = "Input here (ESC to cancel): ";
            input_div.append($("<span>").append(prompt));
            input_div.append($("<span class='input_dialog_box'>").append(input));
            show_input_dialog("#input_dialog");
        }

        input.focus();
        if (input_data.select_prefill)
            input.select();

        function send_input_line(finalChar) {
            var input = find_input();
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
            if (input_data.tag !== "repeat")
                comm.send_message("key", { keycode: 21 });
            comm.send_message("input", { text: text });
            close_input();
        }

        input.keydown(function (ev) {
            var input = find_input();
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
            var input = find_input();
            if (input_data.tag == "stash_search")
            {
                if (String.fromCharCode(ev.which) == "?" && input.val().length === 0)
                {
                    ev.preventDefault();
                    comm.send_message("key", { keycode: ev.which });
                    return false;
                }
            }
            else if (input_data.tag == "repeat")
            {
                var ch = String.fromCharCode(ev.which);
                if (ch != "" && !/^\d$/.test(ch))
                {
                    send_input_line(ev.which);
                    ev.preventDefault();
                    return false;
                }
            }
            else if (input_data.tag == "travel_depth")
            {
                var ch = String.fromCharCode(ev.which);
                if ("<>?$^-p".indexOf(ch) != -1)
                {
                    send_input_line(ev.which);
                    ev.preventDefault();
                    return false;
                }
            }
            else if (input_data.tag == "skill_target")
            {
                var ch = String.fromCharCode(ev.which);
                if (ch == "-") // clear selected target
                {
                    send_input_line(ev.which);
                    ev.preventDefault();
                    return false;
                }
            }
        });
    }

    function input_active()
    {
        if (!input_data)
            return false;
        var input = find_input();
        if (!input)
            return false;
        if (!input.is(":visible"))
            return false;

        input.focus(); // side effect!
        return true;

    }

    function input_type()
    {
        if (!input_data)
            return null;
        return input_data.type;
    }

    // see client.show_dialog and client.hide_dialog
    function show_input_dialog(id)
    {
        var elem = $(id);
        elem.stop(true, true).fadeIn(100, function () {
            elem.focus();
        });
        client.center_element(elem);
        input_data.in_overlay = $("#overlay").is(":visible");
        $("#overlay").show();
    }

    function hide_input_dialog(id)
    {
        $("#input_dialog").blur().hide();
        if (!input_data.in_overlay)
            $("#overlay").hide();
    }

    // Message handlers

    function init_input(data)
    {
        if (client.is_watching != null && client.is_watching())
            return;

        input_data = data;

        assert(data.tag !== "repeat" || !data.prefill);

        display_input();
    }

    function close_input()
    {
        if (input_data)
        {
            var input = find_input();
            if (input)
            {
                input.blur();
                if (input_data.type == "messages")
                    input.remove();
                else // "generic"
                    hide_input_dialog();
            }
        }

        input_cleanup();
    }

    comm.register_handlers({
        "init_input": init_input,
        "close_input": close_input
    });


    function handle_size_change()
    {
        if (!input_data)
            return;

        client.center_element($("#input_dialog"));
    }

    $(document).off("game_init.textinput")
               .on("game_init.textinput", function ()
    {
        $(window).off("resize.textinput")
                 .on("resize.textinput", handle_size_change);
    });


    return {
        input_active: input_active,
        find_input: find_input,
        input_type: input_type
    };

});
