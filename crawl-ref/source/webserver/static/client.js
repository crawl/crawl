var current_layer = "crt";

var dungeon_ctx;
var dungeon_cell_w = 32, dungeon_cell_h = 32;
var dungeon_cols = 0, dungeon_rows = 0;

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

var layout_parameters;
var current_layout;

function layout(params)
{
    layout_parameters = params;
    current_layout = undefined;

    do_layout();

    socket.send("\n"); // To end the layout process
}

function do_layout()
{
    var window_width = $(window).width();
    var window_height = $(window).height();

    if (!layout_parameters)
        return false;

    var layer = current_layer;
    set_layer("normal");

    // Determine width of stats area
    var old_html = $("#stats").html();
    var s = "";
    for (var i = 0; i < layout_parameters.stat_width; i++)
        s = s + "&nbsp;";
    $("#stats").html(s);
    var stat_width_pixels = $("#stats").outerWidth();
    $("#stats").html(old_html);

    // Determine height of messages area
    old_html = $("#messages").html();
    s = "";
    for (var i = 0; i < layout_parameters.msg_min_height; i++)
        s = s + "<br>";
    $("#messages").html(s);
    var msg_height_pixels = $("#messages").outerHeight();
    $("#messages").html(old_html);

    // We have to subtract a bit more for scrollbars and margins
    var remaining_width = window_width - stat_width_pixels - 50;
    var remaining_height = window_height - msg_height_pixels - 20;

    // Determine the maximum size for the CRT layer
    set_layer("crt");
    old_html = $("#crt").html();
    var test_size = 20;
    s = "";
    for (var i = 0; i < test_size; i++)
        s = "&nbsp;" + s + "<br>";
    $("#crt").html(s);
    var char_w = $("#crt").width() / test_size;
    var char_h = $("#crt").height() / test_size;
    $("#crt").html(old_html);

    set_layer(layer);

    var layout = {
        stats_height: 24,
        msg_width: 80
    };

    layout.crt_width = Math.floor((window_width - 30) / char_w);
    layout.crt_height = Math.floor((window_height - 15) / char_h);

    layout.view_width = Math.floor(remaining_width / dungeon_cell_w);
    layout.view_height = Math.floor(remaining_height / dungeon_cell_h);
    // TODO: Scale so that everything fits
    if (layout.view_width < layout_parameters.show_diameter)
        layout.view_width = layout_parameters.show_diameter;
    if (layout.view_height < layout_parameters.show_diameter)
        layout.view_height = layout_parameters.show_diameter;

    if (current_layout &&
        layout.stats_height == current_layout.stats_height &&
        layout.crt_width == current_layout.crt_width &&
        layout.crt_height == current_layout.crt_height &&
        layout.msg_width == current_layout.msg_width &&
        layout.view_width == current_layout.view_width &&
        layout.view_height == current_layout.view_height)
        return false;

    log(current_layout);
    log(layout);

    current_layout = layout;

    send_layout(layout);
    return true;
}

function send_layout(layout)
{
    var msg = "";
    msg += "^w" + layout.view_width + "\n";
    msg += "^h" + layout.view_height + "\n";
    msg += "^s" + layout.stats_height + "\n";
    msg += "^W" + layout.crt_width + "\n";
    msg += "^H" + layout.crt_height + "\n";
    msg += "^m" + layout.msg_width + "\n";
    socket.send(msg);
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

function get_img(id)
{
    return $("#" + id)[0];
}

function view_size(cols, rows)
{
    if ((cols == dungeon_cols) && (rows == dungeon_rows))
        return;

    log("Changing view size to: " + cols + "/" + rows);
    dungeon_cols = cols;
    dungeon_rows = rows;
    canvas = $("#dungeon")[0];
    canvas.width = dungeon_cols * dungeon_cell_w;
    canvas.height = dungeon_rows * dungeon_cell_h;
    dungeon_ctx = canvas.getContext("2d");

    clear_tile_cache();

    var dungeon_offset = $("#dungeon").offset();

    $("#stats").offset({
        left: dungeon_offset.left + canvas.width + 10,
        top: dungeon_offset.top
    });
}

function shift(cx, cy)
{
    var x = cx * dungeon_cell_w;
    var y = cy * dungeon_cell_h;

    var w = dungeon_cols, h = dungeon_rows;

    var sx, sy, dx, dy;

    if (x > 0)
    {
        sx = x;
        dx = 0;
    }
    else
    {
        sx = 0;
        dx = -x;
    }
    if (y > 0)
    {
        sy = y;
        dy = 0;
    }
    else
    {
        sy = 0;
        dy = -y;
    }
    w = (w * dungeon_cell_w - abs(x));
    h = (h * dungeon_cell_h - abs(y));

    dungeon_ctx.drawImage($("#dungeon")[0], sx, sy, w, h, dx, dy, w, h);

    dungeon_ctx.fillStyle = "black";
    dungeon_ctx.fillRect(0, 0, w * dungeon_cell_w, dy);
    dungeon_ctx.fillRect(0, dy, dx, h);
    dungeon_ctx.fillRect(w, 0, sx, h * dungeon_cell_h);
    dungeon_ctx.fillRect(0, h, w, sy);

    // Shift the tile cache
    shift_tile_cache(cx, cy);

    // Shift cursors
    $.each(cursor_locs, function(type, loc)
           {
               if (loc)
               {
                   loc.x -= cx;
                   loc.y -= cy;
               }
           });

    // Shift overlays
    $.each(overlaid_locs, function(i, loc)
           {
               if (loc)
               {
                   loc.x -= cx;
                   loc.y -= cy;
               }
           });
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
        if ($.inArray(String.fromCharCode(e.which), captured_control_keys) == -1)
            return;

        e.preventDefault();
        var code = e.which - "A".charCodeAt(0) + 1; // Compare the CONTROL macro in defines.h
        socket.send("\\" + code + "\n");
    }
    else if (!e.ctrlKey && !e.shiftKey && e.altKey)
    {
        var s = String.fromCharCode(e.which);
        socket.send("\\27\n" + s);
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

function crawl_ended()
{
    set_layer("lobby");
    current_layout = undefined;
}

var watching = false;
function set_watching(val)
{
    watching = val;
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
        if (current_user == undefined)
            set_layer("lobby");
        else
        {
            var game_id = play[1];
            socket.send("Play: " + game_id);
        }
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

        if ("WebSocket" in window)
        {
            // socket_server is set in the client.html template
            socket = new WebSocket(socket_server);

            socket.onopen = function()
            {
                window.onhashchange = hash_changed;

                if (location.hash == "" ||
                    location.hash.match(/^#lobby$/i))
                    set_layer("lobby");
                else
                    hash_changed();

                start_login();
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
