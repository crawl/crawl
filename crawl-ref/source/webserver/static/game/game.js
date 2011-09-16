var dungeon_renderer;

var minimap_ctx;
var minimap_cell_w, minimap_cell_h;
var minimap_x = 0, minimap_y = 0;
var minimap_display_x = 0, minimap_display_y = 0;

$(document).ready(function () {
    dungeon_renderer = new DungeonViewRenderer($("#dungeon")[0]);
});

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

    // Determine width/height of stats area
    var old_html = $("#stats").html();
    var s = "";
    for (var i = 0; i < layout_parameters.stat_width; i++)
        s = s + "&nbsp;";
    for (var i = 0; i < layout_parameters.min_stat_height; i++)
        s = s + "<br>";
    $("#stats").html(s);
    var stat_width_pixels = $("#stats").outerWidth();
    var stat_height_pixels = $("#stats").outerHeight();
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

    var layout = {
        stats_height: layout_parameters.min_stat_height,
        msg_width: 80
    };

    layout.crt_width = Math.floor((window_width - 30) / char_w);
    layout.crt_height = Math.floor((window_height - 15) / char_h);

    // Position controls
    set_layer("normal");
    var minimap_display = $("#minimap").css("display");
    $("#minimap, #minimap_overlay").show();

    dungeon_renderer.fit_to(remaining_width, remaining_height,
                            layout_parameters.show_diameter);

    var minimap_block = $("#minimap_block");
    var minimap_canvas = $("#minimap")[0];
    minimap_block.width(stat_width_pixels);
    minimap_canvas.width = stat_width_pixels;
    minimap_cell_w = minimap_cell_h = Math.floor(stat_width_pixels / layout_parameters.gxm);
    minimap_block.height(layout_parameters.gym * minimap_cell_h);
    minimap_canvas.height = layout_parameters.gym * minimap_cell_h;
    minimap_ctx = minimap_canvas.getContext("2d");

    var minimap_overlay = $("#minimap_overlay")[0];
    minimap_overlay.width = minimap_canvas.width;
    minimap_overlay.height = minimap_canvas.height;

    // Go back to the old layer, re-hide the minimap if necessary
    $("#minimap, #minimap_overlay").css("display", minimap_display);
    set_layer(layer);

    // Update the view
    force_full_render(true);
    display();
    update_minimap_overlay();

    // Send the layout
    if (current_layout &&
        layout.stats_height == current_layout.stats_height &&
        layout.crt_width == current_layout.crt_width &&
        layout.crt_height == current_layout.crt_height &&
        layout.msg_width == current_layout.msg_width)
        return false;

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

function set_dungeon_cell_size(w, h)
{
    dungeon_renderer.set_cell_size(w, h);
    do_layout();
}

// View area -------------------------------------------------------------------
function vgrdc(x, y)
{
    dungeon_renderer.set_view_center(x, y);

    update_minimap_overlay();
}

function update_minimap_overlay()
{
    // Update the minimap overlay
    var minimap_overlay = $("#minimap_overlay")[0];
    var ctx = minimap_overlay.getContext("2d");
    var view = dungeon_renderer.view;
    ctx.clearRect(0, 0, minimap_overlay.width, minimap_overlay.height);
    ctx.strokeStyle = "yellow";
    ctx.strokeRect(minimap_display_x + (view.x - minimap_x) * minimap_cell_w + 0.5,
                   minimap_display_y + (view.y - minimap_y) * minimap_cell_h + 0.5,
                   dungeon_renderer.cols * minimap_cell_w - 1,
                   dungeon_renderer.rows * minimap_cell_h - 1);
}
