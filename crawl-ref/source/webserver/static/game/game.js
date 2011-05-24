var dungeon_ctx;
var dungeon_cell_w = 32, dungeon_cell_h = 32;
var dungeon_cols = 0, dungeon_rows = 0;

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
