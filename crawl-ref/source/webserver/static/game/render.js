var overlaid_locs = [];
var cursor_locs = [];
var minimap_bounds = undefined;
var minimap_changed = false;
var flash = 0;
var minimap_enabled = true;

var ascii_mode = false;

// Debug helper
var mark_sent_cells = false;
function mark_cell(x, y, mark)
{
    mark = mark || "m";

    if (get_tile_cache(x, y))
        get_tile_cache(x, y).mark = mark;

    render_cell(x, y);
}
function unmark_cell(x, y)
{
    var cell = get_tile_cache(x, y);
    if (cell)
    {
        delete cell.mark;
    }

    render_cell(x, y);
}
function mark_all()
{
    for (var x = 0; x < dungeon_cols; x++)
        for (var y = 0; y < dungeon_rows; y++)
            mark_cell(view_x + x, view_y + y, (view_x + x) + "/" + (view_y + y));
}
function unmark_all()
{
    for (var x = 0; x < dungeon_cols; x++)
        for (var y = 0; y < dungeon_rows; y++)
            unmark_cell(view_x + x, view_y + y);
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
    clear_tile_cache();
    dungeon_ctx.fillStyle = "black";
    dungeon_ctx.fillRect(0, 0,
                         dungeon_cols * dungeon_cell_w,
                         dungeon_rows * dungeon_cell_h);

    minimap_ctx.fillStyle = "black";
    minimap_ctx.fillRect(0, 0,
                         $("#minimap").width(),
                         $("#minimap").height());

    minimap_bounds = undefined;
}

function mappable(val)
{
    minimap_enabled = val;
    $("#minimap,#minimap_overlay").toggle(!!val);
}

function c(x, y, cell)
{
    var old_cell = get_tile_cache(x, y);

    if (old_cell && old_cell.sy && (old_cell.sy < 0))
    {
        var above = get_tile_cache(x, y - 1) || {};
        above.dirty = true;
    }

    if (!old_cell || cell.c)
    {
        set_tile_cache(x, y, cell);
    }
    else
    {
        for (attr in cell)
            old_cell[attr] = cell[attr];
        cell = old_cell;
    }

    cell.dirty = true;

    if (minimap_bounds)
    {
        if (minimap_bounds.left > x)
        {
            minimap_bounds.left = x;
            minimap_changed = true;
        }
        if (minimap_bounds.right < x)
        {
            minimap_bounds.right = x;
            minimap_changed = true;
        }
        if (minimap_bounds.top > y)
        {
            minimap_bounds.top = y;
            minimap_changed = true;
        }
        if (minimap_bounds.bottom < y)
        {
            minimap_bounds.bottom = y;
            minimap_changed = true;
        }
    }
    else
    {
        minimap_bounds = {
            left: x,
            top: y,
            right: x,
            bottom: y
        };
    }

    if (mark_sent_cells)
        cell.mark = x + "/" + y;
}

function display()
{
    if (minimap_bounds == undefined)
        return;

    if (minimap_changed)
    {
        center_minimap();
        minimap_changed = false;
    }

    // Make sure we call render_cell at least for the whole visible area
    // so that the flash covers the whole canvas
    var l = minimap_bounds.left;
    if (l > view_x) l = view_x;
    var r = minimap_bounds.right;
    if (r < view_x + dungeon_cols) r = view_x + dungeon_cols - 1;
    var t = minimap_bounds.top;
    if (t > view_y) t = view_y;
    var b = minimap_bounds.bottom;
    if (b < view_y + dungeon_rows) b = view_y + dungeon_rows - 1;

    for (var x = l; x <= r; x++)
        for (var y = t; y <= b; y++)
    {
        var cell = get_tile_cache(x, y);
        if (!cell || cell.dirty) render_cell(x, y);
        if (cell) cell.dirty = false;
    }
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
    if (in_view(x, y))
        draw_main(idx, x - view_x, y - view_y);
    overlaid_locs.push({x: x, y: y});
}

function clear_overlays()
{
    $.each(overlaid_locs, function(i, loc)
           {
               render_cell(loc.x, loc.y);
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
        render_cell(old_loc.x, old_loc.y);

    if (in_view(x, y))
        render_cursors(x, y);
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
        render_cell(old_loc.x, old_loc.y);
}

// Render functions
function force_full_render(minimap_too)
{
    if (minimap_bounds == undefined)
        return;

    var xs = minimap_too ? minimap_bounds.left : view_x;
    var ys = minimap_too ? minimap_bounds.top : view_y;
    var xe = minimap_too ? minimap_bounds.right : view_x + dungeon_cols - 1;
    var ye = minimap_too ? minimap_bounds.bottom : view_y + dungeon_rows - 1;
    for (var x = xs; x <= xe; x++)
        for (var y = ys; y <= ye; y++)
    {
        var cell = get_tile_cache(x, y);
        if (cell) cell.dirty = true;
    }
}

function render_cursors(cx, cy)
{
    $.each(cursor_locs, function (type, loc)
           {
               if (loc && (loc.x == cx) && (loc.y == cy))
               {
                   var idx;

                   switch (type)
                   {
                   case CURSOR_TUTORIAL:
                       idx = TILEI_TUTORIAL_CURSOR;
                       break;
                   case CURSOR_MOUSE:
                       idx = TILEI_CURSOR;
                       // TODO: TILEI_CURSOR2 if not visible
                       // But do we even need a server-side mouse cursor?
                       break;
                   case CURSOR_MAP:
                       idx = TILEI_CURSOR;
                       break;
                   }

                   draw_icon(idx, cx - view_x, cy - view_y);
               }
           });
}

function render_cell(cx, cy)
{
    try
    {
        var x = cx - view_x;
        var y = cy - view_y;

        if (in_view(cx, cy))
        {
            dungeon_ctx.fillStyle = "black";
            dungeon_ctx.fillRect(x * dungeon_cell_w, y * dungeon_cell_h,
                                 dungeon_cell_w, dungeon_cell_h);
        }

        var cell = get_tile_cache(cx, cy);

        if (!cell)
        {
            render_flash(x, y);

            render_cursors(cx, cy);
            return;
        }

        cell.sy = undefined; // Will be set by the tile rendering functions
                             // to indicate if the cell is oversized

        cell.fg = cell.fg || 0;
        cell.bg = cell.bg || 0;
        cell.flv = cell.flv || {};
        cell.flv.f = cell.flv.f || 0;
        cell.flv.s = cell.flv.s || 0;

        var minimap_feat = get_cell_map_feature(cell);
        set_minimap(cx, cy, minimap_colours[minimap_feat]);

        if (!in_view(cx, cy))
            return;

        // cell is basically a packed_cell + doll + mcache entries
        draw_background(x, y, cell);

        var fg_idx = cell.fg & TILE_FLAG_MASK;
        var is_in_water = in_water(cell);

        // Canvas doesn't support applying an alpha gradient to an image while drawing;
        // so to achieve the same effect as in local tiles, it would probably be best
        // to pregenerate water tiles with the (inverse) alpha gradient built in.
        // This simply draws the lower half with increased transparency; for now,
        // it looks good enough.

        var draw_dolls = function () {
            if ((fg_idx >= TILE_MAIN_MAX) && cell.doll)
            {
                $.each(cell.doll, function (i, doll_part)
                       {
                           draw_player(doll_part[0], x, y, 0, 0, doll_part[1]);
                       });
            }

            if ((fg_idx >= TILEP_MCACHE_START) && cell.mcache)
            {
                $.each(cell.mcache, function (i, mcache_part)
                       {
                           draw_player(mcache_part[0], x, y, mcache_part[1], mcache_part[2]);
                       });
            }
        }

        if (is_in_water)
        {
            dungeon_ctx.save();
            try
            {
                dungeon_ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                set_nonsubmerged_clip(x, y, 20);

                draw_dolls();
            }
            finally
            {
                dungeon_ctx.restore();
            }

            dungeon_ctx.save();
            try
            {
                dungeon_ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
                set_submerged_clip(x, y, 20);

                draw_dolls();
            }
            finally
            {
                dungeon_ctx.restore();
            }
        }
        else
        {
            dungeon_ctx.save();
            try
            {
                dungeon_ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                draw_dolls();
            }
            finally
            {
                dungeon_ctx.restore();
            }
        }

        draw_foreground(x, y, cell);

        render_flash(x, y);

        render_cursors(cx, cy);

        // Redraw the cell below if it overlapped
        var cell_below = get_tile_cache(cx, cy + 1);
        if (cell_below && cell_below.sy && (cell_below.sy < 0))
            render_cell(cx, cy + 1);

        // Debug helper
        if (cell.mark)
        {
            dungeon_ctx.fillStyle = "red";
            dungeon_ctx.font = "12px monospace";
            dungeon_ctx.textAlign = "center";
            dungeon_ctx.textBaseline = "middle";
            dungeon_ctx.fillText(cell.mark,
                                 (x + 0.5) * dungeon_cell_w, (y + 0.5) * dungeon_cell_h);
        }
    }
    catch (err)
    {
        console.error("Error while drawing cell " + obj_to_str(cell)
                      + " at " + cx + "/" + cy + ": " + err);
    }
}

function render_flash(x, y)
{
    if (flash) // Flash
    {
        var col = flash_colours[flash];
        dungeon_ctx.save();
        try
        {
            dungeon_ctx.fillStyle = "rgb(" + col.r + "," + col.g + "," + col.b + ")";
            dungeon_ctx.globalAlpha = col.a / 255;
            dungeon_ctx.fillRect(x * dungeon_cell_w, y * dungeon_cell_h,
                                 dungeon_cell_w, dungeon_cell_h);
        }
        finally
        {
            dungeon_ctx.restore();
        }
    }
}

function get_cell_map_feature(cell)
{
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

        if (cell.noexp)
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
    else
        return MF_FLOOR;
}

function set_submerged_clip(cx, cy, water_level)
{
    var x = dungeon_cell_w * cx;
    var y = dungeon_cell_h * cy;

    dungeon_ctx.beginPath();
    dungeon_ctx.rect(0, y + water_level, dungeon_cell_w * dungeon_cols,
                     dungeon_cell_h * dungeon_cols - y - water_level);
    dungeon_ctx.clip();
}

function set_nonsubmerged_clip(cx, cy, water_level)
{
    var x = dungeon_cell_w * cx;
    var y = dungeon_cell_h * cy;

    dungeon_ctx.beginPath();
    dungeon_ctx.rect(0, 0, dungeon_cell_w * dungeon_cols, y + water_level);
    dungeon_ctx.clip();
}

// Much of the following is more or less directly copied from tiledgnbuf.cc
function in_water(cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

function draw_blood_overlay(x, y, cell, is_wall)
{
    if (cell.liquefied)

    {
        offset = cell.flv.s % tile_dngn_count(TILE_LIQUEFACTION);
        draw_dngn(TILE_LIQUEFACTION + offset, x, y);
    }
    else if (cell.bloody)

    {
        cell.bloodrot = cell.bloodrot || 0;
        if (is_wall)

        {
            basetile = TILE_WALL_BLOOD_S + tile_dngn_count(TILE_WALL_BLOOD_S)
                * cell.bloodrot;
        }
        else
            basetile = TILE_BLOOD;
        offset = cell.flv.s % tile_dngn_count(basetile);
        draw_dngn(basetile + offset, x, y);
    }
    else if (cell.moldy)
    {
        offset = cell.flv.s % tile_dngn_count(TILE_MOLD);
        draw_dngn(TILE_MOLD + offset, x, y);
    }
    else if (cell.glowing_mold)
    {
        offset = cell.flv.s % tile_dngn_count(TILE_GLOWING_MOLD);
        draw_dngn(TILE_GLOWING_MOLD + offset, x, y);
    }
}

function draw_background(x, y, cell)
{
    var bg = cell.bg;
    var bg_idx = cell.bg & TILE_FLAG_MASK;

    if (cell.swtree && bg_idx > TILE_DNGN_UNSEEN)
        draw_dngn(TILE_DNGN_SHALLOW_WATER, x, y);
    else if (bg_idx >= TILE_DNGN_WAX_WALL)
        draw_dngn(cell.flv.f, x, y); // f = floor

    // Draw blood beneath feature tiles.
    if (bg_idx > TILE_WALL_MAX)
        draw_blood_overlay(x, y, cell);

    if (cell.swtree) // Draw the tree submerged
    {
        dungeon_ctx.save();
        try
        {
            dungeon_ctx.globalAlpha = 1.0;

            set_nonsubmerged_clip(x, y, 20);

            draw_dngn(bg_idx, x, y);
        }
        finally
        {
            dungeon_ctx.restore();
        }

        dungeon_ctx.save();
        try
        {
            dungeon_ctx.globalAlpha = 0.3;
            set_submerged_clip(x, y, 20);

            draw_dngn(bg_idx, x, y);
        }
        finally
        {
            dungeon_ctx.restore();
        }
    }
    else
        draw_dngn(bg_idx, x, y);

    if (bg_idx > TILE_DNGN_UNSEEN)

    {
        if (bg & TILE_FLAG_WAS_SECRET)
            draw_dngn(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        // Draw blood on top of wall tiles.
        if (bg_idx <= TILE_WALL_MAX)
            draw_blood_overlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

        // Draw overlays
        if (cell.ov)
        {
            $.each(cell.ov, function (i, overlay)
                   {
                       draw_dngn(overlay, x, y);
                   });
        }

        if (!(bg & TILE_FLAG_UNSEEN))

        {
            if (bg & TILE_FLAG_KRAKEN_NW)
                draw_dngn(TILE_KRAKEN_OVERLAY_NW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NW)
                draw_dngn(TILE_ELDRITCH_OVERLAY_NW, x, y);
            if (bg & TILE_FLAG_KRAKEN_NE)
                draw_dngn(TILE_KRAKEN_OVERLAY_NE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NE)
                draw_dngn(TILE_ELDRITCH_OVERLAY_NE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SE)
                draw_dngn(TILE_KRAKEN_OVERLAY_SE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SE)
                draw_dngn(TILE_ELDRITCH_OVERLAY_SE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SW)
                draw_dngn(TILE_KRAKEN_OVERLAY_SW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SW)
                draw_dngn(TILE_ELDRITCH_OVERLAY_SW, x, y);
        }

        if (cell.halo == HALO_MONSTER)
            draw_dngn(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))

        {
            if (cell.sanctuary)
                draw_dngn(TILE_SANCTUARY, x, y);
            if (cell.silenced && (cell.halo == HALO_RANGE))
                draw_dngn(TILE_HALO_RANGE_SILENCED, x, y);
            else if (cell.silenced)
                draw_dngn(TILE_SILENCED, x, y);
            else if (cell.halo == HALO_RANGE)
                draw_dngn(TILE_HALO_RANGE, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                draw_dngn(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                draw_dngn(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

        if (bg & TILE_FLAG_RAY)
            draw_dngn(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            draw_dngn(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

function draw_foreground(x, y, cell)
{
    var fg = cell.fg;
    var bg = cell.bg;
    var fg_idx = cell.fg & TILE_FLAG_MASK;
    var is_in_water = in_water(cell);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        var base_idx = cell.base;
        if (is_in_water)
        {
            dungeon_ctx.save();
            try
            {
                dungeon_ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                set_nonsubmerged_clip(x, y, 20);

                if (base_idx)
                    draw_main(base_idx, x, y);

                draw_main(fg_idx, x, y);
            }
            finally
            {
                dungeon_ctx.restore();
            }

            dungeon_ctx.save();
            try
            {
                dungeon_ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
                set_submerged_clip(x, y, 20);

                if (base_idx)
                    draw_main(base_idx, x, y);

                draw_main(fg_idx, x, y);
            }
            finally
            {
                dungeon_ctx.restore();
            }
        }
        else
        {
            if (base_idx)
                draw_main(base_idx, x, y);

            draw_main(fg_idx, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        draw_icon(TILEI_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        draw_icon(TILEI_SOMETHING_UNDER, x, y);

    var status_shift = 0;
    if (fg & TILE_FLAG_MIMIC)
        draw_icon(TILEI_MIMIC, x, y);

    if (fg & TILE_FLAG_BERSERK)
    {
        draw_icon(TILEI_BERSERK, x, y);
        status_shift += 10;
    }

    // Pet mark
    if (fg & TILE_FLAG_ATT_MASK)
    {
        att_flag = fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_PET)
        {
            draw_icon(TILEI_HEART, x, y);
            status_shift += 10;
        }
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
        {
            draw_icon(TILEI_GOOD_NEUTRAL, x, y);
            status_shift += 8;
        }
        else if (att_flag == TILE_FLAG_NEUTRAL)
        {
            draw_icon(TILEI_NEUTRAL, x, y);
            status_shift += 8;
        }
    }

    if (fg & TILE_FLAG_BEH_MASK)
    {
        var beh_flag = fg & TILE_FLAG_BEH_MASK;
        if (beh_flag == TILE_FLAG_STAB)
        {
            draw_icon(TILEI_STAB_BRAND, x, y);
            status_shift += 15;
        }
        else if (beh_flag == TILE_FLAG_MAY_STAB)
        {
            draw_icon(TILEI_MAY_STAB_BRAND, x, y);
            status_shift += 8;
        }
        else if (beh_flag == TILE_FLAG_FLEEING)
        {
            draw_icon(TILEI_FLEEING, x, y);
            status_shift += 4;
        }
    }

    if (fg & TILE_FLAG_POISON)
    {
        draw_icon(TILEI_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_STICKY_FLAME)
    {
        draw_icon(TILEI_STICKY_FLAME, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_INNER_FLAME)
    {
        draw_icon(TILEI_INNER_FLAME, x, y, -status_shift, 0);
        status_shift += 8;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        draw_icon(TILEI_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && ((bg != TILE_FLAG_UNSEEN) || fg))
        draw_icon(TILEI_MESH, x, y);

    if (bg & TILE_FLAG_OOR && ((bg != TILE_FLAG_OOR) || fg))
        draw_icon(TILEI_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && ((bg != TILE_FLAG_MM_UNSEEN) || fg))
        draw_icon(TILEI_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        draw_icon(TILEI_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        draw_icon(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        draw_icon(TILEI_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        draw_icon(TILEI_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        var type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILEI_CURSOR : TILEI_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
            type = TILEI_CURSOR3;

        draw_icon(type, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        var mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            draw_icon(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            draw_icon(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            draw_icon(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            draw_icon(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            draw_icon(TILEI_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        var demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            draw_icon(TILEI_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            draw_icon(TILEI_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            draw_icon(TILEI_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            draw_icon(TILEI_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            draw_icon(TILEI_DEMON_NUM5, x, y);
    }
}


// Helper functions for drawing from specific textures
function get_img(id)
{
    return $("#" + id)[0];
}

function draw_tile(idx, cx, cy, img_name, info_func, ofsx, ofsy, y_max)
{
    var x = dungeon_cell_w * cx;
    var y = dungeon_cell_h * cy;
    var info = info_func(idx);
    var img = get_img(img_name);
    if (!info)
    {
        throw ("Tile not found: " + idx);
    }
    var size_ox = dungeon_cell_w / 2 - info.w / 2;
    var size_oy = dungeon_cell_h - info.h;
    var pos_sy_adjust = (ofsy || 0) + info.oy + size_oy;
    var pos_ey_adjust = pos_sy_adjust + info.ey - info.sy;
    var sy = pos_sy_adjust;
    var ey = pos_ey_adjust;
    if (y_max && y_max < ey)
        ey = y_max;

    if (sy >= ey) return;

    if (sy < 0)
    {
        var cell = get_tile_cache(cx + view_x, cy + view_y);
        if (sy < (cell.sy || 0))
            cell.sy = sy;
    }

    var w = info.ex - info.sx;
    var h = info.ey - info.sy;
    dungeon_ctx.drawImage(img,
                          info.sx, info.sy + sy - pos_sy_adjust,
                          w, h + ey - pos_ey_adjust,
                          x + (ofsx || 0) + info.ox + size_ox, y + sy,
                          w, h + ey - pos_ey_adjust)
}

function draw_dngn(idx, cx, cy)
{
    draw_tile(idx, cx, cy, get_dngn_img(idx), get_dngn_tile_info);
}

function draw_main(idx, cx, cy)
{
    draw_tile(idx, cx, cy, "main", get_main_tile_info);
}

function draw_player(idx, cx, cy, ofsx, ofsy, y_max)
{
    draw_tile(idx, cx, cy, "player", get_player_tile_info, ofsx, ofsy, y_max);
}

function draw_icon(idx, cx, cy, ofsx, ofsy)
{
    draw_tile(idx, cx, cy, "icons", get_icons_tile_info, ofsx, ofsy);
}

function center_minimap()
{
    if (!minimap_enabled) return;

    var minimap = $("#minimap")[0];
    var mm_w = minimap_bounds.right - minimap_bounds.left;
    var mm_h = minimap_bounds.bottom - minimap_bounds.top;

    if (mm_w < 0 || mm_h < 0) return;

    var old_x = minimap_display_x - minimap_x * minimap_cell_w;
    var old_y = minimap_display_y - minimap_y * minimap_cell_h;

    minimap_x = minimap_bounds.left;
    minimap_y = minimap_bounds.top;
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

// Shifts the dungeon view by cx/cy cells.
function shift(x, y)
{
    if (x > dungeon_cols) x = dungeon_cols;
    if (x < -dungeon_cols) x = -dungeon_cols;
    if (y > dungeon_rows) y = dungeon_rows;
    if (y < -dungeon_rows) y = -dungeon_rows;

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

    var cw = dungeon_cell_w;
    var ch = dungeon_cell_h;

    var w = (dungeon_cols - abs(x)) * cw;
    var h = (dungeon_rows - abs(y)) * ch;

    if (w > 0 && h > 0)
    {
        dungeon_ctx.drawImage($("#dungeon")[0],
                              sx * cw, sy * ch, w, h,
                              dx * cw, dy * ch, w, h);
    }

    // Render cells that came into view
    for (var cy = 0; cy < dy; cy++)
        for (var cx = 0; cx < dungeon_cols; cx++)
            render_cell(cx + view_x, cy + view_y);

    for (var cy = dy; cy < dungeon_rows - sy; cy++)
    {
        for (var cx = 0; cx < dx; cx++)
            render_cell(cx + view_x, cy + view_y);
        for (var cx = dungeon_cols - sx; cx < dungeon_cols; cx++)
            render_cell(cx + view_x, cy + view_y);
    }

    for (var cy = dungeon_rows - sy; cy < dungeon_rows; cy++)
        for (var cx = 0; cx < dungeon_cols; cx++)
            render_cell(cx + view_x, cy + view_y);
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
