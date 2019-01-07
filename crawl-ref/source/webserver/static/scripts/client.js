define(["exports", "jquery", "key_conversion", "chat", "comm",
        "contrib/jquery.cookie", "contrib/jquery.tablesorter",
        "contrib/jquery.waitforimages", "contrib/inflate"],
function (exports, $, key_conversion, chat, comm) {

    // Need to keep this global for backwards compatibility :(
    window.current_layer = "crt";

    window.debug_mode = false;

    window.log_messages = false;
    window.log_message_size = false;

    var delay_timeout = undefined;
    var message_inhibit = 0;
    var message_queue = [];

    var watching = false;
    var watching_username;
    var playing = false;
    var logging_in = false;
    var showing_close_message = false;
    var current_hash;
    var exit_reason, exit_message, exit_dump;
    var normal_exit = ["saved", "cancel", "quit", "won", "bailed out", "dead"];

    var send_message = comm.send_message;

    var game_version = null;
    var loaded_modules = null;

    window.log = function (text)
    {
        if (window.console && window.console.log)
            window.console.log(text);
    }

    function handle_message(msg)
    {
        if (typeof msg === "string")
        {
            // Javascript code
            eval(msg);
        }
        else
        {
            comm.handle_message(msg);
        }
    }

    function enqueue_messages(msgtext)
    {
        if (msgtext.match(/^{/))
        {
            // JSON message
            var msgobj;
            try
            {
                // Can't use JSON.parse here, because 0.11 and older send
                // invalid JSON messages
                msgobj = eval("(" + msgtext + ")");
            }
            catch (e)
            {
                console.error("Parsing error:", e);
                console.error("Source message:", msgtext);
                return;
            }
            var msgs = msgobj.msgs;
            if (msgs == null)
                msgs = [ msgobj ];
            for (var i in msgs)
            {
                if (window.log_messages && window.log_messages !== 2)
                    console.log("Message: " + msgs[i].msg, msgs[i]);
                if (!comm.handle_message_immediately(msgs[i]))
                    message_queue.push(msgs[i]);
            }
        }
        else
        {
            // Javascript code
            message_queue.push(msgtext);
        }
        handle_message_backlog();
    }

    function handle_message_backlog()
    {
        while (message_queue.length
               && (message_inhibit == 0))
        {
            var msg = message_queue.shift();
            if (window.debug_mode)
            {
                handle_message(msg);
            }
            else
            {
                try
                {
                    handle_message(msg);
                }
                catch (err)
                {
                    console.error("Error in message:", msg);
                    console.error(err.message);
                    console.error(err.stack);
                }
            }
        }
    }

    function inhibit_messages()
    {
        message_inhibit++;
    }
    function uninhibit_messages()
    {
        if (message_inhibit > 0)
            message_inhibit--;
        handle_message_backlog();
    }

    exports.inhibit_messages = inhibit_messages;
    exports.uninhibit_messages = uninhibit_messages;

    function delay(ms)
    {
        if (!("hidden" in document))
        {
            clearTimeout(delay_timeout);
            inhibit_messages();
            delay_timeout = setTimeout(delay_ended, ms);
        }
    }

    function delay_ended()
    {
        delay_timeout = undefined;
        uninhibit_messages();
    }

    exports.delay = delay;

    function show_prompt(title, footer)
    {
        $("#loader_text").hide();
        $("#prompt_title").html(title);
        $("#prompt_footer").html(footer);
        $("#prompt .login_placeholder").append($("#login"));
        $("#login_form").show();
        $("#login .extra_links").hide();
        $("#username").focus();
        $("#prompt").show();
    }

    function hide_prompt(game)
    {
        $("#prompt").hide();
        $("#lobby .login_placeholder").append($("#login"));
        $("#login .extra_links").show();
        $("#loader_text").show();
    }

    var layers = ["crt", "normal", "lobby", "loader"]

    function in_game()
    {
        return current_layer != "lobby" && current_layer != "loader"
               && !showing_close_message;
    }

    function set_layer(layer)
    {
        if (showing_close_message) return;

        hide_prompt();

        $.each(layers, function (i, l) {
            if (l == layer)
                $("#" + l).show();
            else
                $("#" + l).hide();
        });
        current_layer = layer;

        chat.reset_visibility(in_game());
    }

    function register_layer(name)
    {
        if (layers.indexOf(name) == -1)
            layers.push(name);
    }

    exports.set_layer = set_layer;
    exports.register_layer = register_layer;

    function do_layout()
    {
        $("#loader").height($(window).height());
        if (window.do_layout)
            window.do_layout();
    }

    function handle_keypress(e)
    {
        if (!in_game()) return;
        if ($(document.activeElement).hasClass("text")) return;

        // Fix for FF < 25:
        // https://developer.mozilla.org/en-US/docs/Web/Reference/Events/keydown#preventDefault%28%29_of_keydown_event
        if (e.isDefaultPrevented()) return;

        if (e.ctrlKey || e.altKey)
        {
            // allow AltGr keys on various non-english keyboard layouts, not
            // needed for Mozilla where neither ctrlKey or altKey is set
            if ($.browser.mozilla || !e.ctrlKey || !e.altKey)
            {
                //log("CTRL key: " + e.ctrlKey + " " + e.which
                //    + " " + String.fromCharCode(e.which));
                return;
            }
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

        if (watching)
            return;

        e.preventDefault();

        var s = String.fromCharCode(e.which);
        if (s == "{")
        {
            send_bytes(["{".charCodeAt(0)]);
        }
        else
            send_message("input", { text: s });
    }

    function send_keycode(code)
    {
        /* TODO: Use send_message for these as soon as crawl uses a proper
           JSON parser */
        socket.send('{"msg":"key","keycode":' + code + '}');
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
        socket.send(s);
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
        if ($("#stale_processes_message").is(":visible"))
        {
            e.preventDefault();
            send_message("stop_stale_process_purge");
            hide_dialog();
            return;
        }
        if ($("#force_terminate").is(":visible"))
        {
            e.preventDefault();
            if (e.which == "y".charCodeAt(0))
                force_terminate_yes();
            else
                force_terminate_no();
            return;
        }

        if (!in_game())
        {
            if (e.which == 27)
            {
                e.preventDefault();
                hide_dialog();
            }
            return;
        }
        if ($(document.activeElement).hasClass("text")) return;

        // Give the game a chance to handle the key
        if (!retrigger_event(e, "game_keydown"))
            return;

        if (watching)
        {
            if (!e.ctrlKey && !e.shiftKey && !e.altKey)
            {
               if (e.which == 27)
                {
                    e.preventDefault();
                    location.hash = "#lobby";
                }
                else if (e.which == 123)
                {
                    e.preventDefault();
                    chat.focus();
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
                chat.focus();
            }
            else if (e.which in key_conversion.simple)
            {
                e.preventDefault();
                send_keycode(key_conversion.simple[e.which]);
            }
            //else
            //    log("Key: " + e.which);
        }
    }

    function start_login()
    {
        if (get_login_cookie())
        {
            $("#login_form").hide();
            $("#reg_link").hide();
            $("#forgot_link").hide();
            $("#login_message").html("Logging in...");
            $("#remember_me").attr("checked", true);
            send_message("token_login", {
                cookie: get_login_cookie()
            });
            set_login_cookie(null);
        }
        else
            $("#username").focus();
    }

    var current_user;
    function login()
    {
        $("#login_form").hide();
        $("#reg_link").hide();
        $("#forgot_link").hide();
        $("#login_message").html("Logging in...");
        var username = $("#username").val();
        var password = $("#password").val();
        send_message("login", {
            username: username,
            password: password
        });
        return false;
    }

    function login_failed()
    {
        $("#login_message").html("Login failed.");
        $("#login_form").show();
        $("#reg_link").show();
        $("#forgot_link").show();
    }

    function logged_in(data)
    {
        var username = data.username;
        hide_prompt();
        $("#login_message").html("Logged in as " + username);
        current_user = username;
        hide_dialog();
        $("#login_form").hide();
        $("#reg_link").hide();
        $("#forgot_link").hide();
        $("#logout_link").show();

        chat.reset_visibility(true);
        $("#chat_input").show();
        $("#chat_login_text").hide();

        if ($("#remember_me").attr("checked"))
        {
            send_message("set_login_cookie");
        }

        if (!watching)
        {
            current_hash = null;
            hash_changed();
        }
    }

    function remember_me_click()
    {
        if ($("#remember_me").attr("checked"))
        {
            send_message("set_login_cookie");
        }
        else if (get_login_cookie())
        {
            send_message("forget_login_cookie", {
                cookie: get_login_cookie()
            });
            set_login_cookie(null);
        }
    }

    function set_login_cookie(data)
    {
        if (data)
            $.cookie("login", data.cookie, { expires: data.expires });
        else
            $.cookie("login", null);
    }

    function get_login_cookie()
    {
        return $.cookie("login");
    }

    function logout()
    {
        if (get_login_cookie())
        {
            send_message("forget_login_cookie", {
                cookie: get_login_cookie()
            });
            set_login_cookie(null);
        }
        location.reload();
    }

    function show_dialog(id)
    {
        $(".floating_dialog").hide();
        var elem = $(id);
        elem.stop(true, true).fadeIn(100, function () {
            elem.focus();
        });
        center_element(elem);
        $("#overlay").show();
    }
    function center_element(elem)
    {
        var left = $(window).width() / 2 - elem.outerWidth() / 2;
        if (left < 0) left = 0;
        var top = $(window).height() / 2 - elem.outerHeight() / 2;
        if (top < 0) top = 0;
        if (top > 50) top = 50;
        var offset = elem.offset();
        if (offset.left != left || offset.top != top)
            elem.offset({ left: left, top: top });
    }
    function hide_dialog()
    {
        $(".floating_dialog").blur().hide();
        $("#overlay").hide();
    }
    exports.show_dialog = show_dialog;
    exports.hide_dialog = hide_dialog;
    exports.center_element = center_element;

    function handle_dialog(data)
    {
        var dialog = $("<div class='floating_dialog'>");
        dialog.html(data.html);
        $("body").append(dialog);
        dialog.find("[data-key]").on("click", function (ev) {
            var key = $(this).data("key");
            send_bytes([key.charCodeAt(0)]);
        });
        show_dialog(dialog);
    }

    function possessive(name)
    {
        return name + "'" + (name.slice(-1) === "s" ? "" : "s");
    }

    function exit_reason_message(reason, watched_name)
    {
        if (watched_name)
        {
            switch (reason)
            {
            case "quit":
            case "won":
            case "bailed out":
            case "dead":
                return null;
            case "cancel":
                return watched_name + " quit before creating a character.";
            case "saved":
                return watched_name + " stopped playing (saved).";
            case "crash":
                return possessive(watched_name) + " game crashed.";
            case "error":
                return possessive(watched_name)
                       + " game was terminated due to an error.";
            default:
                return possessive(watched_name) + " game ended unexpectedly."
                       + (reason != "unknown" ? " (" + reason + ")" : "");
            }
        }
        else
        {
            switch (reason)
            {
            case "quit":
            case "won":
            case "bailed out":
            case "dead":
            case "saved":
            case "cancel":
                return null;
            case "crash":
                return "Unfortunately your game crashed.";
            case "error":
                return "Unfortunately your game terminated due to an error.";
            default:
                return "Unfortunately your game ended unexpectedly."
                       + (reason != "unknown" ? " (" + reason + ")" : "");
            }
        }
    }

    function show_exit_dialog(reason, message, dump, watched_name)
    {
        var reason_msg = exit_reason_message(reason, watched_name);
        if (reason_msg != null)
            $("#exit_game_reason").text(reason_msg).show();
        else
            $("#exit_game_reason").hide();

        if (message)
            $("#exit_game_message").text(message.replace(/\s+$/, "")).show();
        else
            $("#exit_game_message").hide();

        if (dump)
        {
            var a = $("<a>").attr("target", "_blank")
                            .attr("href", dump + ".txt");
            if (reason === "saved")
                a.text("Character dump");
            else if (reason === "crash")
                a.text("Crash log");
            else
                a.text("Morgue file");
            $("#exit_game_dump").html(a).show();
        }
        else
            $("#exit_game_dump").hide();

        show_dialog("#exit_game");
    }

    function start_register()
    {
        show_dialog("#register");
        $("#reg_username").focus();
    }

    function cancel_register()
    {
        hide_dialog();
    }

    function register()
    {
        var username = $("#reg_username").val();
        var password = $("#reg_password").val();
        var password_repeat = $("#reg_repeat_password").val();
        var email = $("#reg_email").val();

        if (username.indexOf(" ") >= 0)
        {
            $("#register_message").html("The username can't contain spaces.");
            return false;
        }

        if (password !== password_repeat)
        {
            $("#register_message").html("Passwords don't match.");
            return false;
        }

        send_message("register", {
            username: username,
            password: password,
            email: email
        });

        return false;
    }

    function register_failed(data)
    {
        $("#register_message").html(data.reason);
    }

    function start_forgot_password()
    {
        $("#forgot_message").html("");
        show_dialog("#forgot");
        $("#forgot_email").focus();
    }

    function cancel_forgot_password()
    {
        hide_dialog();
    }

    function forgot_password()
    {
        var email = $("#forgot_email").val();

        if (email.indexOf(" ") >= 0)
        {
            $("#forgot_message").html("The email address can't contain spaces.");
            return false;
        }

        send_message("forgot_password", {
            email: email
        });

        return false;
    }

    function forgot_password_failed(data)
    {
        $("#forgot_message").html(data.reason);
    }

    function forgot_password_done()
    {
        show_dialog("#forgot_2");
    }

    function reset_password()
    {
        var token = $("#reset_pw_token").val();
        var password = $("#reset_pw_password").val();
        var password_repeat = $("#reset_pw_repeat_password").val();

        if (password !== password_repeat)
        {
            $("#reset_pw_message").html("Passwords don't match.");
            return false;
        }

        send_message("reset_password", {
            token: token,
            password: password
        });

        return false;
    }

    function cancel_reset_password()
    {
        do_reload_url()
    }

    function reset_password_failed(data)
    {
        $("#reset_pw_message").html(data.reason);
    }

    var editing_rc;
    function edit_rc(id)
    {
        send_message("get_rc", { game_id: id });
        editing_rc = id;
    }

    function rcfile_contents(data)
    {
        $("#rc_file_contents").val(data.contents);
        show_dialog("#rc_edit");
        $("#rc_file_contents").focus();
    }

    function send_rc()
    {
        send_message("set_rc", {
            game_id: editing_rc,
            contents: $("#rc_file_contents").val()
        });
        hide_dialog();
        return false;
    }

    function pong(data)
    {
        send_message("pong");
        return true;
    }

    function connection_closed(data)
    {
        var msg = data.reason;
        set_layer("crt");
        hide_dialog();
        chat.reset_visibility(false);
        $("#crt").html(msg + "<br><br>");
        showing_close_message = true;
        return true;
    }

    function play_now(id)
    {
        send_message("play", {
            game_id: id
        });
    }

    function show_loading_screen()
    {
        if (current_layer == "loader") return;
        var imgs = $("#loader img");
        imgs.hide();
        var count = imgs.length;
        var rand_index = Math.floor(Math.random() * count);
        $(imgs[rand_index]).show();
        set_layer("loader");
    }

    function cleanup()
    {
        document.title = "WebTiles - Dungeon Crawl Stone Soup";

        hide_dialog();

        $(document).trigger("game_cleanup");
        $("#game").html('<div id="crt" style="display: none;"></div>');

        chat.clear();

        watching = false;
    }

    function go_lobby()
    {
        var was_watching = watching;

        cleanup();
        current_hash = "#lobby";
        location.hash = "#lobby";
        set_layer("lobby");
        $("#username").focus();

        if (exit_reason)
        {
            if (was_watching || normal_exit.indexOf(exit_reason) === -1)
            {
                show_exit_dialog(exit_reason, exit_message, exit_dump,
                                 was_watching ? watching_username : null);
            }
        }
        exit_reason = null;
        exit_message = null;
        exit_dump = null;

        if ( $("#reset_pw").length )
        {
            show_dialog("#reset_pw");
        }
    }

    function login_required(data)
    {
        cleanup();
        show_loading_screen();
        show_prompt("Login required to play <span id='prompt_game'>"
                    + data.game + "</span>:",
                    "<a href='#lobby'>Back to lobby</a>");
    }

    function chat_login(data)
    {
        if (!in_game() || !watching) return;

        var a = $("<a href='javascript:'>Close</a>");
        a.click(hide_prompt);
        show_prompt("Login to chat:", a);
    }

    var new_list = null;

    function lobby_entry(data)
    {
        var single = false;
        if (new_list == null)
        {
            single = true;
            new_list = $("#player_list").clone();
        }

        var id = "game-" + data.id;
        var entry = new_list.find("#" + id);
        if (entry.length == 0)
        {
            entry = $("#game_entry_template").clone();
            entry.attr("id", id);
            new_list.append(entry);
        }

        function set(key, value)
        {
            entry.find("." + key).html(value);
        }

        var username_entry = $(make_watch_link(data));
        username_entry.text(data.username);
        set("username", username_entry);
        set("game_id", data.game_id);
        set("xl", data.xl);
        set("char", data.char);
        set("place", data.place);
        set("god", data.god || "");
        set("title", data.title);
        set("idle_time", format_idle_time(data.idle_time));
        entry.find(".idle_time")
            .data("time", data.idle_time)
            .attr("data-time", "" + data.idle_time);
        entry.find(".idle_time")
            .data("sort", "" + data.idle_time)
            .attr("data-sort", "" + data.idle_time);
        set("spectator_count", data.spectator_count > 0 ? data.spectator_count : "");
        if (entry.find(".milestone").text() !== data.milestone)
        {
            if (single)
                roll_in_new_milestone(entry, data.milestone);
            else
                set("milestone", data.milestone);
        }

        if (single)
            lobby_complete();
    }

    var lobby_idle_timer = setInterval(update_lobby_idle_times, 1000);
    function update_lobby_idle_times()
    {
        $("#player_list .idle_time").each(function () {
            var $this = $(this);
            var time = $this.data("time");
            if (time)
            {
                time++;
                $this.html(format_idle_time(time));
                $this.data("time", time)
                    .attr("data-time", "" + time);
                $this.data("sort", "" + time)
                    .attr("data-sort", "" + time);
            }
        });
    }

    function lobby_remove(data)
    {
        $("#game-" + data.id).remove();
        if ($("#player_list tbody tr").length == 0)
            $("#lobby_body").hide();
    }

    function lobby_clear()
    {
        new_list = $("#player_list").clone();
        new_list.find("tbody").html("");
    }
    function lobby_complete()
    {
        var old_list = $("#player_list");
        var sortlist;
        if (new_list.find("tbody tr").length > 0)
        {
            $("#lobby_body").show();
            if (old_list[0].config)
                sortlist = old_list[0].config.sortList;
            else
                sortlist = [[0, 0]];
            new_list.tablesorter({
                sortList: sortlist,
                textExtraction: extract_text_or_data,
                headers: { 0: { sorter: "text" } }
            });
        }
        else
            $("#lobby_body").hide();
        old_list.replaceWith(new_list);
        new_list = null;
    }

    function make_watch_link(data)
    {
        return "<a href='#watch-" + data.username + "'></a>";
    }

    function format_idle_time(seconds)
    {
        var elem = $("<span></span>");
        if (seconds == 0)
        {
            elem.text("");
        }
        else if (seconds < 120)
        {
            elem.text(seconds + " s");
        }
        else if (seconds < (60 * 60))
        {
            elem.text(Math.round(seconds / 60) + " min");
        }
        else
        {
            elem.text(Math.round(seconds / (60 * 60)) + " h");
        }
        return elem;
    }

    function extract_text_or_data(elem)
    {
        var $elem = $(elem);
        if ($elem.data("sort"))
            return $elem.data("sort");
        else
            return $elem.text();
    }

    function roll_in_new_milestone(row, milestone)
    {
        var td = row.find(".milestone_col");
        if (td.length == 0) return;

        var new_milestone = td.find(".new_milestone");
        new_milestone.text(milestone);

        var milestones = td.find(".new_milestone, .milestone");
        milestones.animate({ top: "-1.1em" }, function () {
            milestones.text(milestone);
            milestones.css({ top: 0 });
        });
    }


    function force_terminate_no()
    {
        send_message("force_terminate", { answer: false });
        hide_dialog();
    }
    function force_terminate_yes()
    {
        send_message("force_terminate", { answer: true });
        hide_dialog();
    }
    function handle_stale_processes(data)
    {
        $(".game_name").html(data.game);
        $(".recover_timeout").html("" + data.timeout);
        show_dialog("#stale_processes_message");
    }
    function handle_force_terminate(data)
    {
        show_dialog("#force_terminate");
    }

    comm.register_handlers({
        "stale_processes": handle_stale_processes,
        "force_terminate?": handle_force_terminate
    });

    function watching_started(data)
    {
        watching = true;
        watching_username = data.username;
        playing = false;
    }
    exports.is_watching = function ()
    {
        return watching;
    }

    function crawl_started()
    {
        playing = true;
        watching = false;
    }

    function crawl_ended(data)
    {
        playing = false;

        exit_reason = data.reason;
        exit_message = data.message;
        exit_dump = data.dump;
    }

    function hash_changed()
    {
        if (location.hash === current_hash)
            return;
        current_hash = location.hash;

        var watch = location.hash.match(/^#watch-(.+)/i);
        var play = location.hash.match(/^#play-(.+)/i);
        if (watch)
        {
            var watch_user = watch[1];
            send_message("watch", {
                username: watch_user
            });
        }
        else if (play)
        {
            var game_id = play[1];
            send_message("play", {
                game_id: game_id
            });
        }
        else
        {
            send_message("go_lobby");
        }
    }

    function set_game_links(data)
    {
        $("#play_now").html(data.content);
        $("#play_now .edit_rc_link").click(function (ev) {
            var id = $(this).data("game_id");
            edit_rc(id);
        });
    }

    function set_html(data)
    {
        $("#" + data.id).html(data.content);
    }

    requirejs.onResourceLoad = function (context, map, depArray) {
        if (loaded_modules != null)
            loaded_modules.push(map.id);
    }

    function receive_game_client(data)
    {
        if (game_version == null || data["version"] != game_version)
        {
            $(document).off();
            bind_document_events();
            for (var i in loaded_modules)
                requirejs.undef(loaded_modules[i]);
            loaded_modules = [];
        }
        game_version = data["version"];

        inhibit_messages();
        show_loading_screen();
        delete window.game_loading;
        $("#game").html(data.content);
        if (data.content.indexOf("game_loading") === -1)
        {
            // old version, uninhibit can happen too early
            $("#game").waitForImages(uninhibit_messages);
        }
        else
        {
            $("#game").waitForImages(function ()
            {
                var load_interval = setInterval(check_loading, 50);

                function check_loading()
                {
                    if (window.game_loading)
                    {
                        delete window.game_loading;
                        uninhibit_messages();
                        clearInterval(load_interval);
                    }
                }
            });
        }
    }

    function do_set_layer(data)
    {
        set_layer(data.layer);
    }

    function do_reload_url()
    {
        window.location.assign('/');
    }

    function handle_multi_message(data)
    {
        var msg;
        while (msg = data.msgs.pop())
            message_queue.unshift(msg);
    }

    function decode_utf8(bufs, callback)
    {
        if ("TextDecoder" in window)
        {
            var decoder = new TextDecoder('utf-8');
            callback(decoder.decode(bufs));
        }
        else
        {
            // this approach is only a fallback for older browsers because the
            // order of the callback isn't guaranteed, so messages can get
            // queued out of order. TODO: maybe just fall back on uncompressed
            // sockets instead?
            var b = new Blob([bufs]);
            var f = new FileReader();
            f.onload = function(e) {
                callback(e.target.result)
            }
            f.readAsText(b, "UTF-8");
        }
    }

    var blob_construction_supported = true;
    try {
        var blob = new Blob([new Uint8Array([0])]);
    }
    catch (e)
    {
        blob_construction_supported = false;
        log("Blob construction not supported, disabling compression");
    }

    function inflate_works_on_ua()
    {
        if (!blob_construction_supported)
            return false;
        var b = $.browser;
        if (b.chrome && b.version.match("^14\."))
            return false; // doesn't support binary frames
        if (b.chrome && b.version.match("^19\."))
            return false; // buggy Blob builder
        if (b.safari)
            return false;
        if (b.opera)
            return false; // errors in inflate.js (?)
        return true;
    }

    function bind_document_events()
    {
        $(document).on("keypress.client", handle_keypress);
        $(document).on("keydown.client", handle_keydown);
    }

    // Global functions for backwards compatibility (HACK)
    window.log = log;
    window.set_layer = set_layer;
    window.assert = function () {};
    window.abs = function (x) { if (x < 0) return -x; else return x; }

    comm.register_immediate_handlers({
        "ping": pong,
        "close": connection_closed,
    });

    comm.register_handlers({
        "multi": handle_multi_message,

        "set_game_links": set_game_links,
        "html": set_html,
        "show_dialog": handle_dialog,
        "hide_dialog": hide_dialog,

        "lobby_clear": lobby_clear,
        "lobby_entry": lobby_entry,
        "lobby_remove": lobby_remove,
        "lobby_complete": lobby_complete,

        "go_lobby": go_lobby,
        "login_required": login_required,
        "game_started": crawl_started,
        "game_ended": crawl_ended,

        "login_success": logged_in,
        "login_fail": login_failed,
        "login_cookie": set_login_cookie,
        "register_fail": register_failed,
        "forgot_password_fail": forgot_password_failed,
        "forgot_password_done": forgot_password_done,
        "reset_password_fail": reset_password_failed,

        "watching_started": watching_started,

        "rcfile_contents": rcfile_contents,

        "game_client": receive_game_client,

        "layer": do_set_layer,

        "reload_url": do_reload_url,
    });

    $(document).ready(function () {
        bind_document_events();

        $(window).resize(function (ev) {
            do_layout();
        });

        $(window).bind("beforeunload", function (ev) {
            if (location.hash.match(/^#play-(.+)/i) &&
                socket.readyState == 1)
            {
                return "Really save and quit the game?";
            }
        });

        $(".hide_dialog").click(hide_dialog);

        $("#login_form").bind("submit", login);
        $("#remember_me").bind("click", remember_me_click);
        $("#logout_link").bind("click", logout);
        $("#chat_login_link").bind("click", chat_login);

        $("#reg_link").bind("click", start_register);
        $("#register_form").bind("submit", register);
        $("#reg_cancel").bind("click", cancel_register);

        $("#forgot_link").bind("click", start_forgot_password);
        $("#forgot_form").bind("submit", forgot_password);
        $("#forgot_cancel").bind("click", cancel_forgot_password);

        $("#forgot_2 input").bind("click", hide_dialog);

        if ( $("#reset_pw").length )
        {
            $("#reset_pw_cancel").bind("click", cancel_reset_password);
            $("#reset_pw_form").bind("submit", reset_password);
        }

        $("#rc_edit_form").bind("submit", send_rc);

        $("#force_terminate_no").click(force_terminate_no);
        $("#force_terminate_yes").click(force_terminate_yes);

        do_layout();

        var inflater = null;

        if ("Uint8Array" in window &&
            "Blob" in window &&
            "FileReader" in window &&
            "ArrayBuffer" in window &&
            inflate_works_on_ua() &&
            !$.cookie("no-compression"))
        {
            inflater = new Inflater();
        }

        if ("MozWebSocket" in window)
        {
            window.WebSocket = MozWebSocket;
        }

        if ("WebSocket" in window)
        {
            // socket_server is set in the client.html template
            if (inflater)
                socket = new WebSocket(socket_server);
            else
                socket = new WebSocket(socket_server, "no-compression");
            socket.binaryType = "arraybuffer";

            socket.onopen = function ()
            {
                window.onhashchange = hash_changed;

                start_login();

                current_hash = null;
                hash_changed();
            };

            socket.onmessage = function (msg)
            {
                if (inflater && msg.data instanceof ArrayBuffer)
                {
                    var data = new Uint8Array(msg.data.byteLength + 4);
                    data.set(new Uint8Array(msg.data), 0);
                    data.set([0, 0, 255, 255], msg.data.byteLength);
                    var decompressed = inflater.append(data);
                    if (decompressed === -1)
                    {
                        console.error("Decompression error!");
                        var x = inflater.append(data);
                    }
                    decode_utf8(decompressed, function (s) {
                        if (window.log_messages === 2)
                            console.log("Message: " + s);
                        if (window.log_message_size)
                        {
                            console.log("Message size: " + s.length
                                + " (compressed " + msg.data.byteLength + ")");
                        }
                        enqueue_messages(s);
                    });
                    return;
                }

                if (window.log_messages === 2)
                    console.log("Message: " + msg.data);
                if (window.log_message_size)
                    console.log("Message size: " + msg.data.length);

                enqueue_messages(msg.data);
            };

            socket.onerror = function ()
            {
                if (!showing_close_message)
                {
                    set_layer("crt");
                    $("#crt").html("");
                    $("#crt").append("The WebSocket connection failed.<br>");
                    showing_close_message = true;
                }
            };

            socket.onclose = function (ev)
            {
                if (!showing_close_message)
                {
                    set_layer("crt");
                    $("#crt").html("");
                    $("#crt").append("The Websocket connection was closed.<br>");
                    if (ev.reason)
                    {
                        $("#crt").append("Reason:<br>" + ev.reason + "<br>");
                    }
                    $("#crt").append("Reload to try again.<br>");
                    showing_close_message = true;
                }
            };
        }
        else
        {
            set_layer("crt");
            $("#crt").html("Sadly, your browser does not support WebSockets. <br><br>");
            $("#crt").append("The following browsers can be used to run WebTiles: ");
            $("#crt").append("<ul><li>Chrome 6 and greater</li>" +
                             "<li>Firefox 4 and 5, after enabling the " +
                             "network.websocket.override-security-block setting in " +
                             "about:config</li><li>Opera 11, after " +
                             " enabling WebSockets in opera:config#Enable%20WebSockets" +
                             "<li>Safari 5</li></ul>");
        }
    });

    function abs(x)
    {
        return x > 0 ? x : -x;
    }

    return exports;
});
