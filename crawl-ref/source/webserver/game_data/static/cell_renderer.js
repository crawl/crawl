function DungeonCellRenderer(element)
{
    this.element = element;

    this.set_cell_size(32, 32);
}

(function () {
    function in_water(cell)
    {
        return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
    }

    function get_img(id)
    {
        return $("#" + id)[0];
    }

    $.extend(DungeonCellRenderer.prototype, {
        init: function()
        {
            this.ctx = this.element.getContext("2d");
        },

        set_cell_size: function(w, h)
        {
            this.cell_width = w;
            this.cell_height = h;
            this.x_scale = w / 32;
            this.y_scale = h / 32;
        },

        render_cursors: function(cx, cy, x, y)
        {
            var renderer = this;
            $.each(cursor_locs, function (type, loc) {
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

                    renderer.draw_icon(idx, x, y);
                }
            });
        },

        render_cell: function(cx, cy, x, y, map_cell, cell)
        {
            try
            {
                this.ctx.fillStyle = "black";
                this.ctx.fillRect(x, y, this.cell_width, this.cell_height);

                map_cell = map_cell || map_knowledge.get(cx, cy);
                cell = cell || map_cell.t;

                if (!cell)
                {
                    this.render_flash(x, y);

                    this.render_cursors(cx, cy, x, y);
                    return;
                }

                this.current_sy = 0;

                cell.fg = cell.fg || 0;
                cell.bg = cell.bg || 0;
                cell.flv = cell.flv || {};
                cell.flv.f = cell.flv.f || 0;
                cell.flv.s = cell.flv.s || 0;

                // cell is basically a packed_cell + doll + mcache entries
                this.draw_background(x, y, cell);

                var fg_idx = cell.fg & TILE_FLAG_MASK;
                var is_in_water = in_water(cell);

                // Canvas doesn't support applying an alpha gradient
                // to an image while drawing; so to achieve the same
                // effect as in local tiles, it would probably be best
                // to pregenerate water tiles with the (inverse) alpha
                // gradient built in.  This simply draws the lower
                // half with increased transparency; for now, it looks
                // good enough.

                var renderer = this;
                var draw_dolls = function () {
                    if ((fg_idx >= TILE_MAIN_MAX) && cell.doll)
                    {
                        $.each(cell.doll, function (i, doll_part)
                               {
                                   renderer.draw_player(doll_part[0],
                                                        x, y, 0, 0, doll_part[1]);
                               });
                    }

                    if ((fg_idx >= TILEP_MCACHE_START) && cell.mcache)
                    {
                        $.each(cell.mcache, function (i, mcache_part)
                               {
                                   renderer.draw_player(mcache_part[0],
                                                        x, y, mcache_part[1], mcache_part[2]);
                               });
                    }
                }

                if (is_in_water)
                {
                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                        this.set_nonsubmerged_clip(x, y, 20);

                        draw_dolls();
                    }
                    finally
                    {
                        this.ctx.restore();
                    }

                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
                        this.set_submerged_clip(x, y, 20);

                        draw_dolls();
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }
                else
                {
                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                        draw_dolls();
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }

                this.draw_foreground(x, y, cell);

                this.render_flash(x, y);

                this.render_cursors(cx, cy, x, y);

                // Debug helper
                if (cell.mark)
                {
                    this.ctx.fillStyle = "red";
                    this.ctx.font = "12px monospace";
                    this.ctx.textAlign = "center";
                    this.ctx.textBaseline = "middle";
                    this.ctx.fillText(cell.mark,
                                      x + 0.5 * this.cell_width,
                                      y + 0.5 * this.cell_height);
                }

                cell.sy = this.current_sy;
            }
            catch (err)
            {
                console.error("Error while drawing cell " + obj_to_str(cell)
                              + " at " + cx + "/" + cy + ": " + err);
            }
        },

        render_flash: function(x, y)
        {
            if (flash) // Flash
            {
                var col = flash_colours[flash];
                this.ctx.save();
                try
                {
                    this.ctx.fillStyle = "rgb(" + col.r + "," +
                        col.g + "," + col.b + ")";
                    this.ctx.globalAlpha = col.a / 255;
                    this.ctx.fillRect(x, y, this.cell_width, this.cell_height);
                }
                finally
                {
                    this.ctx.restore();
                }
            }
        },

        set_submerged_clip: function(x, y, water_level)
        {
            this.ctx.beginPath();
            this.ctx.rect(0, y + water_level * this.y_scale,
                          this.element.width,
                          this.element.height - y - water_level);
            this.ctx.clip();
        },

        set_nonsubmerged_clip: function(x, y, water_level)
        {
            this.ctx.beginPath();
            this.ctx.rect(0, 0, this.element.width, y + water_level * this.y_scale);
            this.ctx.clip();
        },

        // Much of the following is more or less directly copied from tiledgnbuf.cc
        draw_blood_overlay: function(x, y, cell, is_wall)
        {
            if (cell.liquefied)
            {
                offset = cell.flv.s % tile_dngn_count(TILE_LIQUEFACTION);
                this.draw_dngn(TILE_LIQUEFACTION + offset, x, y);
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
                this.draw_dngn(basetile + offset, x, y);
            }
            else if (cell.moldy)
            {
                offset = cell.flv.s % tile_dngn_count(TILE_MOLD);
                this.draw_dngn(TILE_MOLD + offset, x, y);
            }
            else if (cell.glowing_mold)
            {
                offset = cell.flv.s % tile_dngn_count(TILE_GLOWING_MOLD);
                this.draw_dngn(TILE_GLOWING_MOLD + offset, x, y);
            }
        },

        draw_background: function(x, y, cell)
        {
            var bg = cell.bg;
            var bg_idx = cell.bg & TILE_FLAG_MASK;

            if (cell.swtree && bg_idx > TILE_DNGN_UNSEEN)
                this.draw_dngn(TILE_DNGN_SHALLOW_WATER, x, y);
            else if (bg_idx >= TILE_DNGN_WAX_WALL)
                this.draw_dngn(cell.flv.f, x, y); // f = floor

            // Draw blood beneath feature tiles.
            if (bg_idx > TILE_WALL_MAX)
                this.draw_blood_overlay(x, y, cell);

            if (cell.swtree) // Draw the tree submerged
            {
                this.ctx.save();
                try
                {
                    this.ctx.globalAlpha = 1.0;

                    this.set_nonsubmerged_clip(x, y, 20);

                    this.draw_dngn(bg_idx, x, y);
                }
                finally
                {
                    this.ctx.restore();
                }

                this.ctx.save();
                try
                {
                    this.ctx.globalAlpha = 0.3;
                    this.set_submerged_clip(x, y, 20);

                    this.draw_dngn(bg_idx, x, y);
                }
                finally
                {
                    this.ctx.restore();
                }
            }
            else
                this.draw_dngn(bg_idx, x, y);

            if (bg_idx > TILE_DNGN_UNSEEN)
            {
                if (bg & TILE_FLAG_WAS_SECRET)
                    this.draw_dngn(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

                // Draw blood on top of wall tiles.
                if (bg_idx <= TILE_WALL_MAX)
                    this.draw_blood_overlay(x, y, cell, bg_idx >= TILE_FLOOR_MAX);

                // Draw overlays
                if (cell.ov)
                {
                    var renderer = this;
                    $.each(cell.ov, function (i, overlay)
                           {
                               renderer.draw_dngn(overlay, x, y);
                           });
                }

                if (!(bg & TILE_FLAG_UNSEEN))
                {
                    if (bg & TILE_FLAG_KRAKEN_NW)
                        this.draw_dngn(TILE_KRAKEN_OVERLAY_NW, x, y);
                    else if (bg & TILE_FLAG_ELDRITCH_NW)
                        this.draw_dngn(TILE_ELDRITCH_OVERLAY_NW, x, y);
                    if (bg & TILE_FLAG_KRAKEN_NE)
                        this.draw_dngn(TILE_KRAKEN_OVERLAY_NE, x, y);
                    else if (bg & TILE_FLAG_ELDRITCH_NE)
                        this.draw_dngn(TILE_ELDRITCH_OVERLAY_NE, x, y);
                    if (bg & TILE_FLAG_KRAKEN_SE)
                        this.draw_dngn(TILE_KRAKEN_OVERLAY_SE, x, y);
                    else if (bg & TILE_FLAG_ELDRITCH_SE)
                        this.draw_dngn(TILE_ELDRITCH_OVERLAY_SE, x, y);
                    if (bg & TILE_FLAG_KRAKEN_SW)
                        this.draw_dngn(TILE_KRAKEN_OVERLAY_SW, x, y);
                    else if (bg & TILE_FLAG_ELDRITCH_SW)
                        this.draw_dngn(TILE_ELDRITCH_OVERLAY_SW, x, y);
                }

                if (cell.halo == HALO_MONSTER)
                    this.draw_dngn(TILE_HALO, x, y);

                if (!(bg & TILE_FLAG_UNSEEN))
                {
                    if (cell.sanctuary)
                        this.draw_dngn(TILE_SANCTUARY, x, y);
                    if (cell.silenced && (cell.halo == HALO_RANGE))
                        this.draw_dngn(TILE_HALO_RANGE_SILENCED, x, y);
                    else if (cell.silenced)
                        this.draw_dngn(TILE_SILENCED, x, y);
                    else if (cell.halo == HALO_RANGE)
                        this.draw_dngn(TILE_HALO_RANGE, x, y);
                    if (cell.orb_glow)
                        this.draw_dngn(TILE_ORB_GLOW + cell.orb_glow - 1, x, y);

                    // Apply the travel exclusion under the foreground if the cell is
                    // visible.  It will be applied later if the cell is unseen.
                    if (bg & TILE_FLAG_EXCL_CTR)
                        this.draw_dngn(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
                    else if (bg & TILE_FLAG_TRAV_EXCL)
                        this.draw_dngn(TILE_TRAVEL_EXCLUSION_BG, x, y);
                }

                if (bg & TILE_FLAG_RAY)
                    this.draw_dngn(TILE_RAY, x, y);
                else if (bg & TILE_FLAG_RAY_OOR)
                    this.draw_dngn(TILE_RAY_OUT_OF_RANGE, x, y);
            }
        },

        draw_foreground: function(x, y, cell)
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
                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = cell.trans ? 0.5 : 1.0;

                        this.set_nonsubmerged_clip(x, y, 20);

                        if (base_idx)
                            this.draw_main(base_idx, x, y);

                        this.draw_main(fg_idx, x, y);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }

                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = cell.trans ? 0.1 : 0.3;
                        this.set_submerged_clip(x, y, 20);

                        if (base_idx)
                            this.draw_main(base_idx, x, y);

                        this.draw_main(fg_idx, x, y);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }
                else
                {
                    if (base_idx)
                        this.draw_main(base_idx, x, y);

                    this.draw_main(fg_idx, x, y);
                }
            }

            if (fg & TILE_FLAG_NET)
                this.draw_icon(TILEI_TRAP_NET, x, y);

            if (fg & TILE_FLAG_S_UNDER)
                this.draw_icon(TILEI_SOMETHING_UNDER, x, y);

            var status_shift = 0;
            if (fg & TILE_FLAG_MIMIC)
                this.draw_icon(TILEI_MIMIC, x, y);

            if (fg & TILE_FLAG_BERSERK)
            {
                this.draw_icon(TILEI_BERSERK, x, y);
                status_shift += 10;
            }

            // Pet mark
            if (fg & TILE_FLAG_ATT_MASK)
            {
                att_flag = fg & TILE_FLAG_ATT_MASK;
                if (att_flag == TILE_FLAG_PET)
                {
                    this.draw_icon(TILEI_HEART, x, y);
                    status_shift += 10;
                }
                else if (att_flag == TILE_FLAG_GD_NEUTRAL)
                {
                    this.draw_icon(TILEI_GOOD_NEUTRAL, x, y);
                    status_shift += 8;
                }
                else if (att_flag == TILE_FLAG_NEUTRAL)
                {
                    this.draw_icon(TILEI_NEUTRAL, x, y);
                    status_shift += 8;
                }
            }

            if (fg & TILE_FLAG_BEH_MASK)
            {
                var beh_flag = fg & TILE_FLAG_BEH_MASK;
                if (beh_flag == TILE_FLAG_STAB)
                {
                    this.draw_icon(TILEI_STAB_BRAND, x, y);
                    status_shift += 15;
                }
                else if (beh_flag == TILE_FLAG_MAY_STAB)
                {
                    this.draw_icon(TILEI_MAY_STAB_BRAND, x, y);
                    status_shift += 8;
                }
                else if (beh_flag == TILE_FLAG_FLEEING)
                {
                    this.draw_icon(TILEI_FLEEING, x, y);
                    status_shift += 4;
                }
            }

            if (fg & TILE_FLAG_POISON)
            {
                this.draw_icon(TILEI_POISON, x, y, -status_shift, 0);
                status_shift += 5;
            }
            if (fg & TILE_FLAG_STICKY_FLAME)
            {
                this.draw_icon(TILEI_STICKY_FLAME, x, y, -status_shift, 0);
                status_shift += 5;
            }
            if (fg & TILE_FLAG_INNER_FLAME)
            {
                this.draw_icon(TILEI_INNER_FLAME, x, y, -status_shift, 0);
                status_shift += 8;
            }

            if (fg & TILE_FLAG_ANIM_WEP)
                this.draw_icon(TILEI_ANIMATED_WEAPON, x, y);

            if (bg & TILE_FLAG_UNSEEN && ((bg != TILE_FLAG_UNSEEN) || fg))
                this.draw_icon(TILEI_MESH, x, y);

            if (bg & TILE_FLAG_OOR && ((bg != TILE_FLAG_OOR) || fg))
                this.draw_icon(TILEI_OOR_MESH, x, y);

            if (bg & TILE_FLAG_MM_UNSEEN && ((bg != TILE_FLAG_MM_UNSEEN) || fg))
                this.draw_icon(TILEI_MAGIC_MAP_MESH, x, y);

            // Don't let the "new stair" icon cover up any existing icons, but
            // draw it otherwise.
            if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
                this.draw_icon(TILEI_NEW_STAIR, x, y);

            if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
                this.draw_icon(TILEI_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
                this.draw_icon(TILEI_TRAVEL_EXCLUSION_FG, x, y);

            // Tutorial cursor takes precedence over other cursors.
            if (bg & TILE_FLAG_TUT_CURSOR)
            {
                this.draw_icon(TILEI_TUTORIAL_CURSOR, x, y);
            }
            else if (bg & TILE_FLAG_CURSOR)
            {
                var type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
                    TILEI_CURSOR : TILEI_CURSOR2;

                if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
                    type = TILEI_CURSOR3;

                this.draw_icon(type, x, y);
            }

            if (fg & TILE_FLAG_MDAM_MASK)
            {
                var mdam_flag = fg & TILE_FLAG_MDAM_MASK;
                if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
                    this.draw_icon(TILEI_MDAM_LIGHTLY_DAMAGED, x, y);
                else if (mdam_flag == TILE_FLAG_MDAM_MOD)
                    this.draw_icon(TILEI_MDAM_MODERATELY_DAMAGED, x, y);
                else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
                    this.draw_icon(TILEI_MDAM_HEAVILY_DAMAGED, x, y);
                else if (mdam_flag == TILE_FLAG_MDAM_SEV)
                    this.draw_icon(TILEI_MDAM_SEVERELY_DAMAGED, x, y);
                else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
                    this.draw_icon(TILEI_MDAM_ALMOST_DEAD, x, y);
            }

            if (fg & TILE_FLAG_DEMON)
            {
                var demon_flag = fg & TILE_FLAG_DEMON;
                if (demon_flag == TILE_FLAG_DEMON_1)
                    this.draw_icon(TILEI_DEMON_NUM1, x, y);
                else if (demon_flag == TILE_FLAG_DEMON_2)
                    this.draw_icon(TILEI_DEMON_NUM2, x, y);
                else if (demon_flag == TILE_FLAG_DEMON_3)
                    this.draw_icon(TILEI_DEMON_NUM3, x, y);
                else if (demon_flag == TILE_FLAG_DEMON_4)
                    this.draw_icon(TILEI_DEMON_NUM4, x, y);
                else if (demon_flag == TILE_FLAG_DEMON_5)
                    this.draw_icon(TILEI_DEMON_NUM5, x, y);
            }
        },


        // Helper functions for drawing from specific textures
        draw_tile: function(idx, x, y, img_name, info_func, ofsx, ofsy, y_max)
        {
            var info = info_func(idx);
            var img = get_img(img_name);
            if (!info)
            {
                throw ("Tile not found: " + idx);
            }
            var size_ox = 32 / 2 - info.w / 2;
            var size_oy = 32 - info.h;
            var pos_sy_adjust = (ofsy || 0) + info.oy + size_oy;
            var pos_ey_adjust = pos_sy_adjust + info.ey - info.sy;
            var sy = pos_sy_adjust;
            var ey = pos_ey_adjust;
            if (y_max && y_max < ey)
                ey = y_max;

            if (sy >= ey) return;

            if (sy < this.current_sy)
                this.current_sy = sy;

            var w = info.ex - info.sx;
            var h = info.ey - info.sy;
            this.ctx.drawImage(img,
                               info.sx, info.sy + sy - pos_sy_adjust,
                               w, h + ey - pos_ey_adjust,
                               x + ((ofsx || 0) + info.ox + size_ox) * this.x_scale,
                               y + sy * this.y_scale,
                               w * this.x_scale,
                               (h + ey - pos_ey_adjust) * this.y_scale)
        },

        draw_dngn: function(idx, x, y)
        {
            this.draw_tile(idx, x, y, get_dngn_img(idx), get_dngn_tile_info);
        },

        draw_main: function(idx, x, y)
        {
            this.draw_tile(idx, x, y, "main", get_main_tile_info);
        },

        draw_player: function(idx, x, y, ofsx, ofsy, y_max)
        {
            this.draw_tile(idx, x, y, "player",
                           get_player_tile_info, ofsx, ofsy, y_max);
        },

        draw_icon: function(idx, x, y, ofsx, ofsy)
        {
            this.draw_tile(idx, x, y, "icons", get_icons_tile_info, ofsx, ofsy);
        },
    });
}) ();
