var current_layer = "crt";

var socket;

var log_messages = false;
var log_message_size = false;

var delay_timeout = undefined;
var message_queue = [];

var logging_in = false;

var received_close_message = false;

function log(text)
{
    if (window.console && window.console.log)
        window.console.log(text);
}

function delay(ms)
{
    clearTimeout(delay_timeout);
    delay_timeout = setTimeout(delay_ended, ms);
}

function delay_ended()
{
    delay_timeout = undefined;
    while (message_queue.length && !delay_timeout)
    {
        msg = message_queue.shift();
        try
        {
            eval(msg);
        } catch (err)
        {
            console.error("Error in message: " + msg.data + " - " + err);
        }
    }
}

var layers = ["crt", "normal", "lobby"]

function set_layer(layer)
{
    if (received_close_message) return;

    $.each(layers, function (i, l)
           {
               if (l == layer)
                   $("#" + l).show();
               else
                   $("#" + l).hide();
           });
    current_layer = layer;

    lobby(layer == "lobby");
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

    if (e.which == 0) return;
    if (e.which == 8) return; // Backspace gets a keypress in FF, but not Chrome
                              // so we handle it in keydown

    e.preventDefault();

    var s = String.fromCharCode(e.which);
    if (s == "_")
    {
        focus_chat();
    }
    else if (s == "\\")
    {
        socket.send("\\92\n");
    }
    else if (s == "^")
    {
        socket.send("\\94\n");
    }
    else
        socket.send(s);
}

function handle_keydown(e)
{
    if (current_layer == "lobby")
    {
        if (e.which == 27)
        {
            e.preventDefault();
            $("#register").hide();
        }
        return;
    }
    if ($(document.activeElement).hasClass("text")) return;

    if (e.ctrlKey && !e.shiftKey && !e.altKey)
    {
        if (e.which in ctrl_key_conversion)
        {
            e.preventDefault();
            socket.send("\\" + ctrl_key_conversion[e.which] + "\n");
        }
        else if ($.inArray(String.fromCharCode(e.which), captured_control_keys) != -1)
        {
            e.preventDefault();
            var code = e.which - "A".charCodeAt(0) + 1; // Compare the CONTROL macro in defines.h
            socket.send("\\" + code + "\n");
        }
    }
    else if (!e.ctrlKey && e.shiftKey && !e.altKey)
    {
        if (e.which in shift_key_conversion)
        {
            e.preventDefault();
            socket.send("\\" + shift_key_conversion[e.which] + "\n");
        }
    }
    else if (!e.ctrlKey && !e.shiftKey && e.altKey)
    {
        //e.preventDefault();
        //var s = String.fromCharCode(e.which);
        //socket.send("\\27\n" + s);
    }
    else if (!e.ctrlKey && !e.shiftKey && !e.altKey)
    {
        if (e.which == 27 && watching)
        {
            e.preventDefault();
            location.hash = "#lobby";
        }
        else if (e.which in key_conversion)
        {
            e.preventDefault();
            socket.send("\\" + key_conversion[e.which] + "\n");
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
        socket.send("LoginToken: " + get_login_cookie());
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
    socket.send("Login: " + username + " " + password);
    return false;
}

function login_failed()
{
    $("#login_message").html("Login failed.");
    $("#login_form").show();
    $("#reg_link").show();
}

function logged_in(username)
{
    $("#login_message").html("Logged in as " + username);
    current_user = username;
    $("#register").hide();
    $("#login_form").hide();
    $("#reg_link").hide();

    if ($("#remember_me").attr("checked"))
    {
        socket.send("Remember");
    }
}

function remember_me_click()
{
    if ($("#remember_me").attr("checked"))
    {
        socket.send("Remember");
    }
    else
    {
        socket.send("UnRemember: " + get_login_cookie());
        set_login_cookie(null);
    }
}

function set_login_cookie(value, expires)
{
    $.cookie("login", value, { expires: 7 });
}

function get_login_cookie()
{
    return $.cookie("login");
}

function start_register()
{
    $("#register").show();
    var w = $("#register").width();
    var ww = $(window).width();
    $("#register").offset({ left: ww / 2 - w / 2, top: 50 });
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

    socket.send("Register: " + username + " " + email + " " + password);

    return false;
}

function register_failed(message)
{
    $("#register_message").html(message);
}

function ping()
{
    socket.send("Pong");
}

function connection_closed(msg)
{
    set_layer("crt");
    $("#chat").hide();
    $("#crt").html(msg);
    received_close_message = true;
}

function request_redraw()
{
    socket.send("^r");
}

function play_now(id)
{
    socket.send("Play: " + id);
}

var lobby_update_timeout = undefined;
var lobby_update_rate = 30000;
function lobby(enable)
{
    if (enable)
    {
        location.hash = "#lobby";
        document.title = "WebTiles - Dungeon Crawl Stone Soup";

        $("#crt").html("");
        $("#stats").html("");
        $("#messages").html("");

        clear_chat();
    }

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
    socket.send("UpdateLobby");
}

var watching = false;
function set_watching(val)
{
    watching = val;
}

var playing = false;
function crawl_started()
{
    playing = true;
}
function crawl_ended()
{
    set_layer("lobby");
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
        socket.send("Watch: " + watch_user);
    }
    else if (play)
    {
        var game_id = play[1];
        socket.send("Play: " + game_id);
    }
    else if (location.hash.match(/^#lobby$/i))
    {
        socket.send("GoLobby");
    }
}

$(document).ready(
    function()
    {
        // Key handler
        $(document).bind('keypress.client', handle_keypress);
        $(document).bind('keydown.client', handle_keydown);

        $(window).resize(function (ev)
                         {
                             if (do_layout())
                                 request_redraw();
                         });

        $(window).bind("beforeunload",
                       function (ev)
                       {
                           if (location.hash.match(/^#play-(.+)/i) &&
                               socket.readyState == WebSocket.OPEN)
                           {
                               return "Really quit the game?";
                           }
                       });

        if ("WebSocket" in window)
        {
            // socket_server is set in the client.html template
            socket = new WebSocket(socket_server);

            socket.onopen = function()
            {
                window.onhashchange = hash_changed;

                start_login();

                if (location.hash == "" ||
                    location.hash.match(/^#lobby$/i))
                    set_layer("lobby");
                else
                    hash_changed();
            };

            socket.onmessage = function(msg)
            {
                if (log_messages)
                {
                    console.log("Message: " + msg.data);
                }
                if (log_message_size)
                {
                    console.log("Message size: " + msg.data.length);
                }
                if (delay_timeout)
                {
                    message_queue.push(msg.data);
                } else
                {
                    try
                    {
                        eval(msg.data);
                    } catch (err)
                    {
                        console.error("Error in message: " + msg.data + " - " + err);
                    }
                }
            };

            socket.onclose = function()
            {
                if (!received_close_message)
                {
                    set_layer("crt");
                    $("#crt").html("The Websocket connection was closed. Reload to try again.");
                }
            };
        }
        else
        {
            set_layer("crt");
            $("#crt").html("Sadly, your browser does not support WebSockets. <br><br>");
            $("#crt").append("The following browsers can be used to run WebTiles: ");
            $("#crt").append("<ul><li>Chrome 6 and greater</li>" +
                             "<li>Firefox 4, after disabling the " +
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
function assert(cond)
{
    if (!cond)
        console.log("Assertion failed!");
}
