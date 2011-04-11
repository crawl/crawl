
// This gets called by the messages sent by crawl
function c(x, y, cell) {
    try {
        // cell is basically a packed_cell + doll + mcache entries
        drawBackground(x, y, cell);

        fg_idx = cell.fg & TILE_FLAG_MASK;
        in_water = inWater(cell);

        if (cell.doll) {
            $.each(cell.doll, function (i, dollPart) {
                drawPlayer(dollPart[0], x, y); // TODO: dollPart[1] is Y_MAX, use that
            });
        }

        if (cell.mcache) {
            $.each(cell.mcache, function (i, mcachePart) {
                drawPlayer(mcachePart[0], x, y, mcachePart[1], mcachePart[2]);
            });
        }

        drawForeground(x, y, cell);
    } catch (err) {
        console.error("Error while drawing cell " + objectToString(cell)
                      + " at " + x + "/" + y + ": " + err);
    }
}

// Much of the following is more or less directly copied from tiledgnbuf.cc
function inWater(cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

function drawBloodOverlay(x, y, cell, isWall)
{
    if (cell.liquefied)
    {
//        offset = cell.flv.special % tile_dngn_count(TILE_LIQUEFACTION);
        offset = 0;
        drawDngn(TILE_LIQUEFACTION + offset, x, y);
    }
    else if (cell.bloody)
    {
        if (isWall)
        {
            basetile = TILE_WALL_BLOOD_S + tile_dngn_count(TILE_WALL_BLOOD_S)
                                           * cell.bloodrot;
        }
        else
            basetile = TILE_BLOOD;
//        offset = cell.flv.special % tile_dngn_count(basetile);
        offset = 0;
        drawDngn(basetile + offset, x, y);
    }
    else if (cell.moldy)
    {
//        offset = cell.flv.special % tile_dngn_count(TILE_MOLD);
        offset = 0;
        drawDngn(TILE_MOLD + offset, x, y);
    }
}

function drawBackground(x, y, cell)
{
    bg = cell.bg;
    bg_idx = cell.bg & TILE_FLAG_MASK;

    if (cell.swtree && bg_idx > TILE_DNGN_UNSEEN)
        drawDngn(TILE_DNGN_SHALLOW_WATER, x, y);

    // TODO: flavour
//    if (bg_idx >= TILE_DNGN_WAX_WALL)
//        drawDngn(cell.flv.floor, x, y);

    // Draw blood beneath feature tiles.
    if (bg_idx > TILE_WALL_MAX)
        drawBloodOverlay(x, y, cell);

    drawDngn(bg_idx, x, y, cell.swtree);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        if (bg & TILE_FLAG_WAS_SECRET)
            drawDngn(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        // Draw blood on top of wall tiles.
        if (bg_idx <= TILE_WALL_MAX)
            drawBloodOverlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

        // Draw overlays
        if (cell.ov) {
            $.each(cell.ov, function (i, overlay) {
                drawDngn(overlay, x, y);
            });
        }

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (bg & TILE_FLAG_KRAKEN_NW)
                drawDngn(TILE_KRAKEN_OVERLAY_NW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NW)
                drawDngn(TILE_ELDRITCH_OVERLAY_NW, x, y);
            if (bg & TILE_FLAG_KRAKEN_NE)
                drawDngn(TILE_KRAKEN_OVERLAY_NE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_NE)
                drawDngn(TILE_ELDRITCH_OVERLAY_NE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SE)
                drawDngn(TILE_KRAKEN_OVERLAY_SE, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SE)
                drawDngn(TILE_ELDRITCH_OVERLAY_SE, x, y);
            if (bg & TILE_FLAG_KRAKEN_SW)
                drawDngn(TILE_KRAKEN_OVERLAY_SW, x, y);
            else if (bg & TILE_FLAG_ELDRITCH_SW)
                drawDngn(TILE_ELDRITCH_OVERLAY_SW, x, y);
        }

        if (cell.haloed)
            drawDngn(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (cell.sanctuary)
                drawDngn(TILE_SANCTUARY, x, y);
            if (cell.silenced)
                drawDngn(TILE_SILENCED, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                drawDngn(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                drawDngn(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

        if (bg & TILE_FLAG_RAY)
            drawDngn(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            drawDngn(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

function drawForeground(x, y, cell)
{
    fg = cell.fg;
    bg = cell.bg;
    fg_idx = cell.fg & TILE_FLAG_MASK;
    in_water = inWater(cell);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        // TODO: Send known base item
        // base_idx = tileidx_known_base_item(fg_idx);
        base_idx = fg_idx;

        if (in_water)
        {
            /*
            TODO: Submerged stuff
            if (base_idx)
                m_buf_main_trans.add(base_idx, x, y, 0, true, false);
            m_buf_main_trans.add(fg_idx, x, y, 0, true, false);
            */
        }
        else
        {
            if (base_idx)
                drawMain(base_idx, x, y);

            drawMain(fg_idx, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        drawIcon(TILEI_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        drawIcon(TILEI_SOMETHING_UNDER, x, y);

    status_shift = 0;
    if (fg & TILE_FLAG_MIMIC)
        drawIcon(TILEI_MIMIC, x, y);

    if (fg & TILE_FLAG_BERSERK)
    {
        drawIcon(TILEI_BERSERK, x, y);
        status_shift += 10;
    }

    // Pet mark
    if (fg & TILE_FLAG_ATT_MASK)
    {
        att_flag = fg & TILE_FLAG_ATT_MASK;
        if (att_flag == TILE_FLAG_PET)
        {
            drawIcon(TILEI_HEART, x, y);
            status_shift += 10;
        }
        else if (att_flag == TILE_FLAG_GD_NEUTRAL)
        {
            drawIcon(TILEI_GOOD_NEUTRAL, x, y);
            status_shift += 8;
        }
        else if (att_flag == TILE_FLAG_NEUTRAL)
        {
            drawIcon(TILEI_NEUTRAL, x, y);
            status_shift += 8;
        }
    }
    else if (fg & TILE_FLAG_STAB)
    {
        drawIcon(TILEI_STAB_BRAND, x, y);
        status_shift += 15;
    }
    else if (fg & TILE_FLAG_MAY_STAB)
    {
        drawIcon(TILEI_MAY_STAB_BRAND, x, y);
        status_shift += 8;
    }

    if (fg & TILE_FLAG_POISON)
    {
        drawIcon(TILEI_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_FLAME)
    {
        drawIcon(TILEI_FLAME, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        drawIcon(TILEI_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        drawIcon(TILEI_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        drawIcon(TILEI_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        drawIcon(TILEI_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        drawIcon(TILEI_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        drawIcon(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        drawIcon(TILEI_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        drawIcon(TILEI_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILEI_CURSOR : TILEI_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILEI_CURSOR3;

        drawIcon(type, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            drawIcon(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            drawIcon(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            drawIcon(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            drawIcon(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            drawIcon(TILEI_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            drawIcon(TILEI_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            drawIcon(TILEI_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            drawIcon(TILEI_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            drawIcon(TILEI_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            drawIcon(TILEI_DEMON_NUM5, x, y);
    }
}


// Helper functions for drawing from specific textures
function drawDngn(idx, cx, cy, submerged) {
    // TODO: submerged
    x = dungeonCellWidth * cx;
    y = dungeonCellHeight * cy;
    img = getImg(getdngnImg(idx));
    info = getdngnTileInfo(idx);
    if (!info) {
        console.error("Dngn tile not found: " + idx);
        return;
    }
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h, x + info.ox, y + info.oy, w, h);
}

function drawMain(idx, cx, cy) {
    x = dungeonCellWidth * cx;
    y = dungeonCellHeight * cy;
    info = getmainTileInfo(idx);
    img = getImg("main");
    if (!info) {
        throw ("Main tile not found: " + idx);
    }
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h,
                             x + info.ox, y + info.oy, w, h);
}

function drawPlayer(idx, cx, cy, ofsx, ofsy) {
    x = dungeonCellWidth * cx;
    y = dungeonCellHeight * cy;
    info = getplayerTileInfo(idx);
    img = getImg("player");
    if (!info) {
        throw ("Player tile not found: " + idx);
    }
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h,
                             x + info.ox + (ofsx || 0), y + info.oy + (ofsy || 0), w, h);
}

function drawIcon(idx, cx, cy, ofsx, ofsy) {
    x = dungeonCellWidth * cx;
    y = dungeonCellHeight * cy;
    info = geticonsTileInfo(idx);
    img = getImg("icons");
    if (!info) {
        throw ("Icon tile not found: " + idx);
    }
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h,
                             x + info.ox + (ofsx || 0), y + info.oy + (ofsy || 0), w, h);
}

function objectToString (o) {
    var parse = function (_o) {

        var a = [], t;

        for (var p in _o) {

            if (_o.hasOwnProperty(p)) {
                t = _o[p];

                if (t && typeof t == "object") {
                    a[a.length]= p + ":{ " + arguments.callee(t).join(", ") + "}";
                }
                else {
                    if (typeof t == "string") {
                        a[a.length] = [ p+ ": \"" + t.toString() + "\"" ];
                    }
                    else {
                        a[a.length] = [ p+ ": " + t.toString()];
                    }
                }
            }
        }
        return a;
    }
    return "{" + parse(o).join(", ") + "}";
}
