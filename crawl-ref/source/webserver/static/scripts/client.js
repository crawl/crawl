define(["exports", "jquery", "key_conversion", "chat", "comm",
        "contrib/jquery.cookie", "contrib/jquery.tablesorter",
        "contrib/jquery.waitforimages"],
function (exports, $, key_conversion, chat, comm) {

    // Need to keep this global for backwards compatibility :(
    window.current_layer = "crt";

    window.debug_mode = false;

    exports.log_messages = false;
    exports.log_message_size = false;

    var delay_timeout = undefined;
    var message_inhibit = 0;
    var message_queue = [];

    var logging_in = false;

    var showing_close_message = false;

    var send_message = comm.send_message;

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

    function enqueue_message(msgtext)
    {
        if (msgtext.match(/^{/))
        {
            // JSON message
            var msgobj = eval("(" + msgtext + ")");
            if (!comm.handle_message_immediately(msgobj))
                message_queue.push(msgobj);
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
                    console.error("Error in message: " + msg);
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
        clearTimeout(delay_timeout);
        inhibit_messages();
        delay_timeout = setTimeout(delay_ended, ms);
    }

    function delay_ended()
    {
        delay_timeout = undefined;
        uninhibit_messages();
    }

    exports.delay = delay;

    var layers = ["crt", "normal", "lobby", "loader"]

    function set_layer(layer)
    {
        if (showing_close_message) return;

        $.each(layers, function (i, l) {
            if (l == layer)
                $("#" + l).show();
            else
                $("#" + l).hide();
        });
        current_layer = layer;

        lobby(layer == "lobby");
    }

    function do_layout()
    {
        $("#loader").height($(window).height());
        if (window.do_layout)
            window.do_layout();
    }

    function handle_keypress(e)
    {
        if (current_layer == "lobby") return;
        if ($(document.activeElement).hasClass("text")) return;

        if (e.ctrlKey || e.altKey)
        {
            log("CTRL key: " + e.ctrlKey + " " + e.which
                + " " + String.fromCharCode(e.which));
            return;
        }

        if ((e.which == 0) ||
            (e.which == 8) || /* Backspace gets a keypress in FF, but not Chrome
                                 so we handle it in keydown */
            (e.which == 9))   /* Tab gives a keypress in Opera even when it is
                                 suppressed in keydown */
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

    function handle_keydown(e)
    {
        if (current_layer == "lobby")
        {
            if (e.which == 27)
            {
                e.preventDefault();
                $(".floating_dialog").hide();
            }
            return;
        }
        if ($(document.activeElement).hasClass("text")) return;

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
            if (e.which == 18) return;

            e.preventDefault();
            send_bytes([27, e.which]);
        }
        else if (!e.ctrlKey && !e.shiftKey && !e.altKey)
        {
            if (e.which == 27 && watching)
            {
                e.preventDefault();
                location.hash = "#lobby";
            }
            else if (e.which == 123)
            {
                e.preventDefault();
                chat.focus();
            }
            else if (e.which in key_conversion.simple)
            {
                e.preventDefault();
                send_keycode(key_conversion.simple[e.which]);
            }
            else
                log("Key: " + e.which);
        }
    }

    function start_login()
    {
        if (get_login_cookie())
        {
            $("#login_form").hide();
            $("#reg_link").hide();
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
    }

    function logged_in(data)
    {
        var username = data.username;
        $("#login_message").html("Logged in as " + username);
        current_user = username;
        $("#register").hide();
        $("#login_form").hide();
        $("#reg_link").hide();
        $("#logout_link").show();

        if ($("#remember_me").attr("checked"))
        {
            send_message("set_login_cookie");
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
        $(id).fadeIn(100, function () {
            $(id).focus();
        });
        var w = $(id).width();
        var ww = $(window).width();
        $(id).offset({ left: ww / 2 - w / 2, top: 50 });
    }

    function start_register()
    {
        show_dialog("#register");
        $("#reg_username").focus();
    }

    function cancel_register()
    {
        $("#register").hide();
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

        if (email.indexOf(" ") >= 0)
        {
            $("#register_message").html("The email address can't contain spaces.");
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
        $("#rc_edit").hide();
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
        $("#chat").hide();
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
        var imgs = $("#loader img");
        imgs.hide();
        var count = imgs.length;
        var rand_index = Math.floor(Math.random() * count);
        $(imgs[rand_index]).show();
        set_layer("loader");
    }

    var lobby_update_timeout = undefined;
    var lobby_update_rate = 30000;

    function go_lobby()
    {
        document.title = "WebTiles - Dungeon Crawl Stone Soup";
        location.hash = "#lobby";

        set_layer("lobby");

        $("#game").html('<div id="crt" style="display: none;"></div>');

        $("#username").focus();

        chat.clear();

        watching = false;

        lobby_update();
    }

    function lobby(enable)
    {
        if (enable && lobby_update_timeout == undefined)
        {
            lobby_update_timeout = setInterval(lobby_update, lobby_update_rate);
        }
        else if (!enable && lobby_update_timeout != undefined)
        {
            clearInterval(lobby_update_timeout);
        }

        $("#chat").toggle(!enable);
    }
    function lobby_update()
    {
        send_message("update_lobby");
    }
    function lobby_data(data)
    {
        var old_list = $("#player_list");
        var l = old_list.clone();
        l.find("tbody").html(data.content);
        if (data.content.trim() != "")
        {
            var sortlist;
            if (old_list[0].config)
                sortlist = old_list[0].config.sortList;
            else
                sortlist = [[0, 0]];
            l.tablesorter({
                sortList: sortlist,
                headers: {
                    3: {
                        sorter: "timespan"
                    }
                }
            });
        }
        old_list.replaceWith(l);
    }


    function force_terminate_no()
    {
        send_message("force_terminate", { answer: false });
        $("#force_terminate").hide().blur();
    }
    function force_terminate_yes()
    {
        send_message("force_terminate", { answer: true });
        $("#force_terminate").hide().blur();
    }
    function stale_processes_keydown(ev)
    {
        ev.preventDefault();
        send_message("stop_stale_process_purge");
        $("#stale_processes_message").hide().blur();
    }
    function force_terminate_keydown(ev)
    {
        ev.preventDefault();
        if (ev.which == "y".charCodeAt(0))
            force_terminate_yes();
        else
            force_terminate_no();
    }
    function handle_stale_processes(data)
    {
        $(".game_name").html(data.game);
        $(".recover_timeout").html("" + data.timeout);
        show_dialog("#stale_processes_message");
    }
    function handle_stale_process_fail(data)
    {
        $("#message_box").html(data.content);
        show_dialog("#message_box");
    }
    function handle_force_terminate(data)
    {
        show_dialog("#force_terminate");
    }

    comm.register_handlers({
        "stale_processes": handle_stale_processes,
        "stale_process_fail": handle_stale_process_fail,
        "force_terminate?": handle_force_terminate
    });

    var watching = false;
    function watching_started()
    {
        watching = true;
    }

    var playing = false;
    function crawl_started()
    {
        playing = true;
    }
    function crawl_ended()
    {
        go_lobby();
        current_layout = undefined;
        playing = false;
    }

    function hash_changed()
    {
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
        else if (location.hash.match(/^#lobby$/i))
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

    function receive_game_client(data)
    {
        inhibit_messages();
        show_loading_screen();
        $("#game").html(data.content);
        $(document).ready(function () {
            $("#game").waitForImages(uninhibit_messages);
        });
    }

    function do_set_layer(data)
    {
        set_layer(data.layer);
    }

    function handle_multi_message(data)
    {
        var msg;
        while (msg = data.msgs.pop())
            message_queue.unshift(msg);
    }



    // Global functions for backwards compatibility (HACK)
    window.log = log;
    window.set_layer = set_layer;
    window.assert = function () {};

    comm.register_immediate_handlers({
        "ping": pong,
        "close": connection_closed,
    });

    comm.register_handlers({
        "multi": handle_multi_message,

        "set_game_links": set_game_links,
        "html": set_html,
        "lobby": lobby_data,

        "go_lobby": go_lobby,
        "game_started": crawl_started,
        "game_ended": crawl_ended,

        "login_success": logged_in,
        "login_fail": login_failed,
        "login_cookie": set_login_cookie,
        "register_fail": register_failed,

        "watching_started": watching_started,

        "rcfile_contents": rcfile_contents,

        "game_client": receive_game_client,

        "layer": do_set_layer,
    });

    $(document).ready(function () {
        // Key handler
        $(document).bind('keypress.client', handle_keypress);
        $(document).bind('keydown.client', handle_keydown);

        $(window).resize(function (ev) {
            do_layout();
        });

        $(window).bind("beforeunload", function (ev) {
            if (location.hash.match(/^#play-(.+)/i) &&
                socket.readyState == 1)
            {
                return "Really quit the game?";
            }
        });

        $("#login_form").bind("submit", login);
        $("#remember_me").bind("click", remember_me_click);
        $("#logout_link").bind("click", logout);

        $("#reg_link").bind("click", start_register);
        $("#register_form").bind("submit", register);
        $("#reg_cancel").bind("click", cancel_register);

        $("#rc_edit_form").bind("submit", send_rc);

        $("#lobby_update_link").bind("click", lobby_update);

        $("#force_terminate_no").click(force_terminate_no);
        $("#force_terminate_yes").click(force_terminate_yes);
        $("#stale_processes_message").keydown(stale_processes_keydown);
        $("#force_terminate").keydown(force_terminate_keydown);

        do_layout();

        if ("MozWebSocket" in window)
        {
            window.WebSocket = MozWebSocket;
        }

        if ("WebSocket" in window)
        {
            // socket_server is set in the client.html template
            socket = new WebSocket(socket_server);

            socket.onopen = function ()
            {
                window.onhashchange = hash_changed;

                start_login();

                if (location.hash == "" ||
                    location.hash.match(/^#lobby$/i))
                    go_lobby();
                else
                    hash_changed();
            };

            socket.onmessage = function (msg)
            {
                if (exports.log_messages)
                {
                    console.log("Message: " + msg.data);
                }
                if (exports.log_message_size)
                {
                    console.log("Message size: " + msg.data.length);
                }
                enqueue_message(msg.data);
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

            $.tablesorter.addParser({
                id: 'timespan',
                is: function(s) {
                    return false;
                },
                format: function(s) {
                    return s.substring(0, s.length - 1);
                },
                type: 'numeric'
            });
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
