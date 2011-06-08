var dungeon_ctx;
var dungeon_cell_w = 32, dungeon_cell_h = 32;
var dungeon_cols = 0, dungeon_rows = 0;
var view_x = 0, view_y = 0;
var view_center_x = 0, view_center_y = 0;

// Text area handling ----------------------------------------------------------
function get_text_area_line(name, line)
{
    var area = $("#" + name);
    var current_line = line;
    var append = "";
    do
    {
        var line_span = $("#" + name + "-" + current_line);
        if (line_span[0])
            break;

        append = "<span id='" + name + "-" + current_line + "'></span>" + append;

        if (current_line == 0)
        {
            // first line didn't exist, so this wasn't handled by this function before
            // -> clean up first
            $("#" + name).html("");
            break;
        }
        else
        {
            append = "<br>" + append;
        }

        current_line--;
    }
    while (true);

    if (append !== "")
        $("#" + name).append(append);

    return $("#" + name + "-" + line);
}

function set_text_area_line(name, line, content)
{
    get_text_area_line(name, line).html(content);
}

function txt(name, lines)
{
    for (line in lines)
        set_text_area_line(name, line, lines[line]);
}

// Layout ----------------------------------------------------------------------
var layout_parameters;
var current_layout;

function layout(params, need_response)
{
    layout_parameters = params;
    current_layout = undefined;

    do_layout();

    if (need_response)
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

    view_width = Math.floor(remaining_width / dungeon_cell_w);
    view_height = Math.floor(remaining_height / dungeon_cell_h);
    // TODO: Scale so that everything fits
    if (view_width < layout_parameters.show_diameter)
        view_width = layout_parameters.show_diameter;
    if (view_height < layout_parameters.show_diameter)
        view_height = layout_parameters.show_diameter;

    view_size(view_width, view_height);

    if (current_layout &&
        layout.stats_height == current_layout.stats_height &&
        layout.crt_width == current_layout.crt_width &&
        layout.crt_height == current_layout.crt_height &&
        layout.msg_width == current_layout.msg_width)
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
    msg += "^s" + layout.stats_height + "\n";
    msg += "^W" + layout.crt_width + "\n";
    msg += "^H" + layout.crt_height + "\n";
    msg += "^m" + layout.msg_width + "\n";
    socket.send(msg);
}

// View area -------------------------------------------------------------------
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

    var dungeon_offset = $("#dungeon").offset();

    $("#stats").offset({
        left: dungeon_offset.left + canvas.width + 10,
        top: dungeon_offset.top
    });

    for (var y = 0; y < dungeon_rows; y++)
        for (var x = 0; x < dungeon_cols; x++)
            render_cell(x + view_x, y + view_y);
    vgrdc(view_center_x, view_center_y);
}

function vgrdc(x, y)
{
    view_center_x = x;
    view_center_y = y;
    var old_vx = view_x, old_vy = view_y;
    view_x = x - Math.floor(dungeon_cols / 2);
    view_y = y - Math.floor(dungeon_rows / 2);
    shift(view_x - old_vx, view_y - old_vy);
}

function in_view(cx, cy)
{
    return (cx >= view_x) && (view_x + dungeon_cols > cx) &&
        (cy >= view_y) && (view_y + dungeon_rows > cy);
}
