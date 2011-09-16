var overlaid_locs = [];
var cursor_locs = [];
var flash = 0;
var minimap_enabled = true;

var ascii_mode = false;

// Debug helper
function mark_cell(x, y, mark)
{
    mark = mark || "m";

    if (map_knowledge.get(x, y).t)
        map_knowledge.get(x, y).t.mark = mark;

    dungeon_renderer.render_loc(x, y);
}
function unmark_cell(x, y)
{
    var cell = map_knowledge.get(x, y);
    if (cell)
    {
        delete cell.t.mark;
    }

    dungeon_renderer.render_loc(x, y);
}
function mark_all()
{
    var view = dungeon_renderer.view;
    for (var x = 0; x < dungeon_renderer.cols; x++)
        for (var y = 0; y < dungeon_renderer.rows; y++)
            mark_cell(view.x + x, view.y + y, (view.x + x) + "/" + (view.y + y));
}
function unmark_all()
{
    var view = dungeon_renderer.view;
    for (var x = 0; x < dungeon_renderer.cols; x++)
        for (var y = 0; y < dungeon_renderer.rows; y++)
            unmark_cell(view.x + x, view.y + y);
}

function VColour(r, g, b, a)
{
    return {r: r, g: g, b: b, a: a};
}

// Compare tilereg-dgn.cc
var flash_colours =
[
    VColour(  0,   0,   0,   0), // BLACK (transparent)
    VColour(  0,   0, 128, 100), // BLUE
    VColour(  0, 128,   0, 100), // GREEN
    VColour(  0, 128, 128, 100), // CYAN
    VColour(128,   0,   0, 100), // RED
    VColour(150,   0, 150, 100), // MAGENTA
    VColour(165,  91,   0, 100), // BROWN
    VColour( 50,  50,  50, 150), // LIGHTGRAY
    VColour(  0,   0,   0, 150), // DARKGRAY
    VColour( 64,  64, 255, 100), // LIGHTBLUE
    VColour( 64, 255,  64, 100), // LIGHTGREEN
    VColour(  0, 255, 255, 100), // LIGHTCYAN
    VColour(255,  64,  64, 100), // LIGHTRED
    VColour(255,  64, 255, 100), // LIGHTMAGENTA
    VColour(150, 150,   0, 100), // YELLOW
    VColour(255, 255, 255, 100), // WHITE
];

// This gets called by the messages sent by crawl
function clear_map()
{
    map_knowledge.clear();

    dungeon_renderer.clear();

    minimap_ctx.fillStyle = "black";
    minimap_ctx.fillRect(0, 0,
                         $("#minimap").width(),
                         $("#minimap").height());

    monster_list.clear();
}

function mappable(val)
{
    minimap_enabled = val;
    $("#minimap,#minimap_overlay").toggle(!!val);
}

function m(data)
{
    map_knowledge.merge(data);

    // Mark cells above high cells as dirty
    $.each(map_knowledge.dirty().slice(), function (i, loc) {
        var cell = map_knowledge.get(loc.x, loc.y);
        if (cell.t && cell.t.sy && cell.t.sy < 0)
            map_knowledge.touch(loc.x, loc.y - 1);
    });

    display();
}

function display()
{
    if (!map_knowledge.bounds())
        return;

    if (map_knowledge.reset_bounds_changed())
    {
        center_minimap();
    }

    var dirty_locs = map_knowledge.dirty();
    for (var i = 0; i < dirty_locs.length; i++)
    {
        var loc = dirty_locs[i];
        var cell = map_knowledge.get(loc.x, loc.y);
        cell.dirty = false;
        monster_list.update_loc(loc);
        dungeon_renderer.render_loc(loc.x, loc.y, cell);
    }
    map_knowledge.reset_dirty();

    monster_list.update();
}

function set_flash(colour)
{
    if (colour != flash)
    {
        force_full_render();
    }
    flash = colour;
}

function add_overlay(idx, x, y)
{
    dungeon_renderer.draw_overlay(idx, x, y);
    overlaid_locs.push({x: x, y: y});
}

function clear_overlays()
{
    $.each(overlaid_locs, function(i, loc)
           {
               dungeon_renderer.render_loc(loc.x, loc.y);
           });
    overlaid_locs = [];
}

function place_cursor(type, x, y)
{
    var old_loc = undefined;
    if (cursor_locs[type])
    {
        old_loc = cursor_locs[type];
    }

    cursor_locs[type] =
        {x: x, y: y};

    if (old_loc)
        dungeon_renderer.render_loc(old_loc.x, old_loc.y);

    dungeon_renderer.render_loc(x, y);
}

function remove_cursor(type)
{
    var old_loc = undefined;
    if (cursor_locs[type])
    {
        old_loc = cursor_locs[type];
    }

    cursor_locs[type] = undefined;

    if (old_loc)
        dungeon_renderer.render_loc(old_loc.x, old_loc.y);
}

// Render functions
function force_full_render(minimap_too)
{
    var b = map_knowledge.bounds();
    if (!b) return;

    var view = dungeon_renderer.view;

    var xs = minimap_too ? b.left : view.x;
    var ys = minimap_too ? b.top : view.y;
    var xe = minimap_too ? b.right : view.x + dungeon_renderer.cols - 1;
    var ye = minimap_too ? b.bottom : view.y + dungeon_renderer.rows - 1;
    for (var x = xs; x <= xe; x++)
        for (var y = ys; y <= ye; y++)
    {
        map_knowledge.touch(x, y);
    }
}

function get_cell_map_feature(map_cell)
{
    var cell = map_cell.t;
    var fg_idx = cell.fg & TILE_FLAG_MASK;
    var bg_idx = cell.bg & TILE_FLAG_MASK;

    // TODO: This needs feature data
    // Or at least send the minimap feature. Using tile data is very hacky

    if (fg_idx == TILEP_PLAYER)
        return MF_PLAYER;
    else if (fg_idx >= TILE_MAIN_MAX)
    {
        var att_flag = cell.fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_NEUTRAL)
            return MF_MONS_NEUTRAL;
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
            return MF_MONS_PEACEFUL;
        else if (att_flag == TILE_FLAG_PET)
            return MF_MONS_FRIENDLY;

        if (map_cell.mon && map_cell.mon.typedata.no_exp)
            return MF_MONS_NO_EXP;
        else
            return MF_MONS_HOSTILE;
    }
    else if (fg_idx >= TILE_CLOUD_FIRE_0 && fg_idx <= TILE_CLOUD_RAGING_WINDS_1)
        return MF_FEATURE;
    else if (fg_idx > 0)
        return MF_ITEM;
    else if (cell.bg & TILE_FLAG_EXCL_CTR && (cell.bg & TILE_FLAG_UNSEEN))
        return MF_EXCL_ROOT;
    else if (cell.bg & TILE_FLAG_TRAV_EXCL && (cell.bg & TILE_FLAG_UNSEEN))
        return MF_EXCL;
    else if (cell.bg & TILE_FLAG_WATER)
        return MF_WATER;
    else if (bg_idx > TILE_WALL_MAX)
        return MF_FEATURE;
    else if (bg_idx >= TILE_FLOOR_MAX)
        return MF_WALL;
    else if (bg_idx == 0)
        return MF_UNSEEN;
    else
        return MF_FLOOR;
}

function center_minimap()
{
    if (!minimap_enabled) return;

    var minimap = $("#minimap")[0];
    var bounds = map_knowledge.bounds();
    var mm_w = bounds.right - bounds.left;
    var mm_h = bounds.bottom - bounds.top;

    if (mm_w < 0 || mm_h < 0) return;

    var old_x = minimap_display_x - minimap_x * minimap_cell_w;
    var old_y = minimap_display_y - minimap_y * minimap_cell_h;

    minimap_x = bounds.left;
    minimap_y = bounds.top;
    minimap_display_x = Math.floor((minimap.width - minimap_cell_w * mm_w) / 2);
    minimap_display_y = Math.floor((minimap.height - minimap_cell_h * mm_h) / 2);

    var dx = minimap_display_x - minimap_x * minimap_cell_w - old_x;
    var dy = minimap_display_y - minimap_y * minimap_cell_h - old_y;

    minimap_ctx.drawImage(minimap,
                          0, 0, minimap.width, minimap.height,
                          dx, dy, minimap.width, minimap.height);

    minimap_ctx.fillStyle = "black";
    if (dx > 0)
        minimap_ctx.fillRect(0, 0, dx, minimap.height);
    if (dx < 0)
        minimap_ctx.fillRect(minimap.width + dx, 0, -dx, minimap.height);
    if (dy > 0)
        minimap_ctx.fillRect(0, 0, minimap.width, dy);
    if (dy < 0)
        minimap_ctx.fillRect(0, minimap.height + dy, minimap.width, -dy);

    update_minimap_overlay();
}

function set_minimap(cx, cy, colour)
{
    if (!minimap_enabled) return;

    var x = cx - minimap_x;
    var y = cy - minimap_y;
    minimap_ctx.fillStyle = colour;
    minimap_ctx.fillRect(minimap_display_x + x * minimap_cell_w,
                         minimap_display_y + y * minimap_cell_h,
                         minimap_cell_w, minimap_cell_h);
}

function tile_flags_to_string(flags)
{
    var s = "";
    for (var v in window)
    {
        if (v.match(/^TILE_FLAG_/) && (flags & window[v]))
            s = s + " " + v;
    }
    return s;
}

function obj_to_str (o)
{
    var parse = function (_o)
    {
        var a = [], t;

        for (var p in _o)
        {
            if (_o.hasOwnProperty(p))
            {
                t = _o[p];

                if (t && typeof t == "object")
                {
                    a[a.length]= p + ":{ " + arguments.callee(t).join(", ") + "}";
                }
                else
                {
                    if (typeof t == "string")
                    {
                        a[a.length] = [ p+ ": \"" + t + "\"" ];
                    }
                    else
                    {
                        a[a.length] = [ p+ ": " + t];
                    }
                }
            }
        }
        return a;
    }
    return "{" + parse(o).join(", ") + "}";
}
