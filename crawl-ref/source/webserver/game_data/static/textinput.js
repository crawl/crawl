define(["jquery", "comm", "client", "./enums", "./util", "./options", "./ui"],
function ($, comm, client, enums, util, options, ui) {
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
        else if (input_data.type == "seed-selection")
            return $(".seed-selection .seed-input input");
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
        else if (input_data.type == "seed-selection")
        {
            // n.b. called from ui-layouts.js, not from client
            var input_div = $(".seed-selection .seed-input").last();
            if (prompt)
                input_div.append($("<span>").append(prompt));
            // TODO: what is this class?
            input_div.append($("<span class='input_dialog_box'>").append(input));
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
            ui.show_popup("#input_dialog");
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
            // ctrl-u + ctr-k to wipe any pre-fill
            if (input_data.tag !== "repeat")
            {
                comm.send_message("key", { keycode: 21 });
                comm.send_message("key", { keycode: 11 });
            }
            comm.send_message("input", { text: text });
            // seed-selection handles this on its own, because there is first
            // a validation step -- the input loop only terminates on abort
            // or if there is an actual number to be found.
            if (input_data.type != "seed-selection")
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
            else if (input_data.type == "seed-selection")
            {
                var ch = String.fromCharCode(ev.which);
                if (ch == "-"     // clear input
                    || ch == "?"  // help
                    || ch == "d") // daily seed
                {
                    comm.send_message("key", { keycode: ev.which });
                    ev.preventDefault();
                    return false;
                }
                // mimic keyfunc on crawl side: prevent non-numbers
                if ("0123456789".indexOf(ch) == -1)
                {
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
                if (input_data.type == "messages"
                    || input_data.type == "seed-selection")
                {
                    input.remove();
                }
                else // "generic"
                    ui.hide_popup();
            }
        }

        input_cleanup();
    }

    function update_input(msg)
    {
        if (input_data)
        {
            var input = find_input();
            if (input)
            {
                input.val(msg.input_text);
                if (msg.select)
                    input.select();
            }
        }
    }

    comm.register_handlers({
        "init_input": init_input,
        "close_input": close_input,
        "update_input": update_input
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
