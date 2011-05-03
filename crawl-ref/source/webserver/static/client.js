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

function set_layer(layer)
{
    log("Setting layer: " + layer);
    if (layer == "crt")
    {
        $("#crt").show();
        $("#dungeon").hide();
        $("#stats").hide();
        $("#messages").hide();
    }
    else if (layer == "normal")
    {
        $("#crt").hide();
        $("#dungeon").show();
        // jQuery should restore display correctly -- but doesn't
        $("#stats").css("display", "inline-block");
        $("#messages").show();
    }
    current_layer = layer;
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
    if (logging_in) return;

    if (e.ctrlKey || e.altKey)
    {
        log("CTRL key: " + e.ctrlKey + " " + e.which
            + " " + String.fromCharCode(e.which));
        return;
    }

    e.preventDefault();

    var s = String.fromCharCode(e.which);
    if (s == "\\")
    {
        socket.send("\\92\n");
    } else if (s == "^")
    {
        socket.send("\\94\n");
    }
    else
        socket.send(s);
}

function handle_keydown(e)
{
    if (logging_in) return;

    if (e.ctrlKey)
    {
        if ($.inArray(String.fromCharCode(e.which), captured_control_keys) == -1)
            return;

        e.preventDefault();
        var code = e.which - "A".charCodeAt(0) + 1; // Compare the CONTROL macro in defines.h
        socket.send("\\" + code + "\n");
    }
    else
    {
        // TODO: Shift and Ctrl?
        if (e.which in key_conversion)
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
    logging_in = true;
    $("#login").show();
    $("#username").focus();
}

function login()
{
    logging_in = false;
    $("#login").hide();
    var username = $("#username").val();
    var password = $("#password").val();
    socket.send("Login: " + username + " " + password);
    return false;
}

function login_failed()
{
    $("#login_message").html("Login failed.");
    start_login();
}

var received_close_message = false;

function connection_closed(msg)
{
    set_layer("crt");
    $("#crt").html(msg);
    received_close_message = true;
}

$(document).ready(
    function()
    {
        set_layer("crt");

        // Key handler
        $(document).bind('keypress.client', handle_keypress);
        $(document).bind('keydown.client', handle_keydown);

        if ("WebSocket" in window)
        {
            // socket_server is set in the client.html template
            socket = new WebSocket(socket_server);

            socket.onopen = function()
            {
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
                    $("#crt").html("Websocket connection was closed. Reloading shortly...");
                }
                setTimeout("window.location.reload()", 3000);
            };
        }
        else
        {
            $("#crt").html("Sadly, your browser does not support WebSockets. ");
            if ($.browser.mozilla)
            {
                $("#crt").append("If you are using Firefox 4, WebSocket support is ");
                $("#crt").append("disabled for security reasons. You can enable it in ");
                $("#crt").append("about:config with the network.websocket");
                $("#crt").append(".override-security-block setting.");
            }
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
