define(["jquery", "./view_data", "./tileinfo-main", "./tileinfo-player",
        "./tileinfo-icons", "./tileinfo-dngn", "./enums",
        "./map_knowledge", "./tileinfos"],
function ($, view_data, main, player, icons, dngn, enums, map_knowledge, tileinfos) {
    function DungeonCellRenderer()
    {
        this.set_cell_size(32, 32);
        this.display_mode = "tiles";
        this.glyph_mode_font_size = 24;
        this.glyph_mode_font = "monospace";
    }

    function in_water(cell)
    {
        return ((cell.bg & enums.TILE_FLAG_WATER) && !(cell.fg & enums.TILE_FLAG_FLYING));
    }

    function split_term_colour(col)
    {
        var fg = col & 0xF;
        var bg = 0;
        var attr = (col & 0xF0) >> 4;
        var param = (col & 0xF000) >> 12;
        return { fg: fg, bg: bg, attr: attr };
    }

    function term_colour_apply_attributes(col)
    {
        if (col.attr == enums.CHATTR.HILITE)
        {
            col.bg = col.param;
        }
        if (col.attr == enums.CHATTR.REVERSE)
        {
            var z = col.bg;
            col.bg = col.fg;
            col.fg = z;
        }
    }

    function get_img(id)
    {
        return $("#" + id)[0];
    }

    function obj_to_str(o)
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
    };

    $.extend(DungeonCellRenderer.prototype, {
        init: function(element)
        {
            this.element = element;
            this.ctx = this.element.getContext("2d");
        },

        set_cell_size: function(w, h)
        {
            this.cell_width = w;
            this.cell_height = h;
            this.x_scale = w / 32;
            this.y_scale = h / 32;
        },

        glyph_mode_font_name: function ()
        {
            return (this.glyph_mode_font_size + "px " +
                    this.glyph_mode_font);
        },

        render_cursors: function(cx, cy, x, y)
        {
            var renderer = this;
            $.each(view_data.cursor_locs, function (type, loc) {
                if (loc && (loc.x == cx) && (loc.y == cy))
                {
                    var idx;

                    switch (type)
                    {
                    case enums.CURSOR_TUTORIAL:
                        idx = icons.TUTORIAL_CURSOR;
                        break;
                    case enums.CURSOR_MOUSE:
                        idx = icons.CURSOR;
                        // TODO: tilei.CURSOR2 if not visible
                        break;
                    case enums.CURSOR_MAP:
                        idx = icons.CURSOR;
                        break;
                    }

                    renderer.draw_icon(idx, x, y);
                }
            });
        },

        do_render_cell: function(cx, cy, x, y, map_cell, cell)
        {
            this.ctx.fillStyle = "black";
            this.ctx.fillRect(x, y, this.cell_width, this.cell_height);

            map_cell = map_cell || map_knowledge.get(cx, cy);
            cell = cell || map_cell.t;

            if (!cell)
            {
                if (this.display_mode != "glyphs")
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
            map_cell.g = map_cell.g || ' ';
            if (map_cell.col == undefined) map_cell.col = 7;

            if (this.display_mode == "glyphs")
            {
                this.render_glyph(x, y, map_cell);

                this.render_cursors(cx, cy, x, y);
                return;
            }

            // cell is basically a packed_cell + doll + mcache entries
            if (this.display_mode == "tiles")
                this.draw_background(x, y, cell);

            var fg_idx = cell.fg & enums.TILE_FLAG_MASK;
            var is_in_water = in_water(cell);

            // Canvas doesn't support applying an alpha gradient
            // to an image while drawing; so to achieve the same
            // effect as in local tiles, it would probably be best
            // to pregenerate water tiles with the (inverse) alpha
            // gradient built in.  This simply draws the lower
            // half with increased transparency; for now, it looks
            // good enough.

            var renderer = this;
            function draw_dolls()
            {
                if ((fg_idx >= main.MAIN_MAX) && cell.doll)
                {
                    $.each(cell.doll, function (i, doll_part) {
                        renderer.draw_player(doll_part[0],
                                             x, y, 0, 0, doll_part[1]);
                    });
                }

                if ((fg_idx >= player.MCACHE_START) && cell.mcache)
                {
                    $.each(cell.mcache, function (i, mcache_part) {
                        if (mcache_part) {
                            renderer.draw_player(mcache_part[0],
                                                 x, y, mcache_part[1], mcache_part[2]);
                        }
                    });
                }
            }

            if (is_in_water && this.display_mode == "tiles")
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
            else if (this.display_mode == "tiles")
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

            this.draw_foreground(x, y, map_cell);

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
        },

        render_cell: function()
        {
            if (window.debug_mode)
                this.do_render_cell.apply(this, arguments);
            else
            {
                try
                {
                    this.do_render_cell.apply(this, arguments);
                }
                catch (err)
                {
                    var cx = arguments[0];
                    var cy = arguments[1];
                    var cell = arguments[5];
                    console.error("Error while drawing cell " + obj_to_str(cell)
                                  + " at " + cx + "/" + cy + ": " + err);
                }
            }
        },

        render_glyph: function (x, y, map_cell, omit_bg)
        {
            var col = split_term_colour(map_cell.col);
            if (omit_bg && col.attr == enums.CHATTR.REVERSE)
                col.attr = 0;
            term_colour_apply_attributes(col);

            var prefix = "";
            if (col.attr == enums.CHATTR.BOLD)
            {
                prefix = "bold ";
            }

            if (!omit_bg)
            {
                this.ctx.fillStyle = enums.term_colours[col.bg];
                this.ctx.fillRect(x, y, this.cell_width, this.cell_height);
            }
            this.ctx.fillStyle = enums.term_colours[col.fg];
            this.ctx.font = prefix + this.glyph_mode_font_name();
            this.ctx.textAlign = "center";
            this.ctx.textBaseline = "middle";

            this.ctx.save();

            try
            {
                this.ctx.beginPath();
                this.ctx.rect(x, y, this.cell_width, this.cell_height);
                this.ctx.clip();

                this.ctx.fillText(map_cell.g,
                                  x + this.cell_width/2, y + this.cell_height/2);
            }
            finally
            {
                this.ctx.restore();
            }
        },

        render_flash: function(x, y)
        {
            if (view_data.flash) // Flash
            {
                var col = view_data.flash_colour;
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
                offset = cell.flv.s % dngn.tile_count(dngn.LIQUEFACTION);
                this.draw_dngn(dngn.LIQUEFACTION + offset, x, y);
            }
            else if (cell.bloody)

            {
                cell.bloodrot = cell.bloodrot || 0;
                var basetile;
                if (is_wall)
                {
                    basetile = dngn.WALL_BLOOD_S + dngn.tile_count(dngn.WALL_BLOOD_S)
                        * cell.bloodrot;
                }
                else
                    basetile = dngn.BLOOD;
                offset = cell.flv.s % dngn.tile_count(basetile);
                this.draw_dngn(basetile + offset, x, y);
            }
            else if (cell.moldy)
            {
                offset = cell.flv.s % dngn.tile_count(dngn.MOLD);
                this.draw_dngn(dngn.MOLD + offset, x, y);
            }
            else if (cell.glowing_mold)
            {
                offset = cell.flv.s % dngn.tile_count(dngn.GLOWING_MOLD);
                this.draw_dngn(dngn.GLOWING_MOLD + offset, x, y);
            }
        },

        draw_background: function(x, y, cell)
        {
            var bg = cell.bg;
            var bg_idx = cell.bg & enums.TILE_FLAG_MASK;

            if (cell.swtree && bg_idx > dngn.DNGN_UNSEEN)
                this.draw_dngn(dngn.DNGN_SHALLOW_WATER, x, y);
            else if (bg_idx >= dngn.DNGN_WAX_WALL)
                this.draw_dngn(cell.flv.f, x, y); // f = floor

            // Draw blood beneath feature tiles.
            if (bg_idx > dngn.WALL_MAX)
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

            if (bg_idx > dngn.DNGN_UNSEEN)
            {
                if (bg & enums.TILE_FLAG_WAS_SECRET)
                    this.draw_dngn(dngn.DNGN_DETECTED_SECRET_DOOR, x, y);

                // Draw blood on top of wall tiles.
                if (bg_idx <= dngn.WALL_MAX)
                    this.draw_blood_overlay(x, y, cell, bg_idx >= dngn.FLOOR_MAX);

                // Draw overlays
                if (cell.ov)
                {
                    var renderer = this;
                    $.each(cell.ov, function (i, overlay)
                           {
                               if (overlay)
                                   renderer.draw_dngn(overlay, x, y);
                           });
                }

                if (!(bg & enums.TILE_FLAG_UNSEEN))
                {
                    if (bg & enums.TILE_FLAG_KRAKEN_NW)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_NW, x, y);
                    else if (bg & enums.TILE_FLAG_ELDRITCH_NW)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_NW, x, y);
                    if (bg & enums.TILE_FLAG_KRAKEN_NE)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_NE, x, y);
                    else if (bg & enums.TILE_FLAG_ELDRITCH_NE)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_NE, x, y);
                    if (bg & enums.TILE_FLAG_KRAKEN_SE)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_SE, x, y);
                    else if (bg & enums.TILE_FLAG_ELDRITCH_SE)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_SE, x, y);
                    if (bg & enums.TILE_FLAG_KRAKEN_SW)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_SW, x, y);
                    else if (bg & enums.TILE_FLAG_ELDRITCH_SW)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_SW, x, y);
                }

                if (cell.halo == enums.HALO_MONSTER)
                    this.draw_dngn(dngn.HALO, x, y);

                if (!(bg & enums.TILE_FLAG_UNSEEN))
                {
                    if (cell.sanctuary)
                        this.draw_dngn(dngn.SANCTUARY, x, y);
                    if (cell.silenced)
                        this.draw_dngn(dngn.SILENCED, x, y);
                    if (cell.halo == enums.HALO_RANGE)
                        this.draw_dngn(dngn.HALO_RANGE, x, y);
                    if (cell.halo == enums.HALO_UMBRA)
                        this.draw_dngn(dngn.UMBRA, x, y);
                    if (cell.orb_glow)
                        this.draw_dngn(dngn.ORB_GLOW + cell.orb_glow - 1, x, y);

                    // Apply the travel exclusion under the foreground if the cell is
                    // visible.  It will be applied later if the cell is unseen.
                    if (bg & enums.TILE_FLAG_EXCL_CTR)
                        this.draw_dngn(dngn.TRAVEL_EXCLUSION_CENTRE_BG, x, y);
                    else if (bg & enums.TILE_FLAG_TRAV_EXCL)
                        this.draw_dngn(dngn.TRAVEL_EXCLUSION_BG, x, y);
                }

                if (bg & enums.TILE_FLAG_RAY)
                    this.draw_dngn(dngn.RAY, x, y);
                else if (bg & enums.TILE_FLAG_RAY_OOR)
                    this.draw_dngn(dngn.RAY_OUT_OF_RANGE, x, y);
            }
        },

        draw_foreground: function(x, y, map_cell)
        {
            var cell = map_cell.t;
            var fg = cell.fg;
            var bg = cell.bg;
            var fg_idx = cell.fg & enums.TILE_FLAG_MASK;
            var is_in_water = in_water(cell);

            if (fg_idx && fg_idx <= main.MAIN_MAX && this.display_mode == "tiles")
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
            else if (this.display_mode == "hybrid")
            {
                this.render_glyph(x, y, map_cell, true);
            }

            if (fg & enums.TILE_FLAG_NET)
                this.draw_icon(icons.TRAP_NET, x, y);

            if (fg & enums.TILE_FLAG_S_UNDER)
                this.draw_icon(icons.SOMETHING_UNDER, x, y);

            var status_shift = 0;
            if (fg & enums.TILE_FLAG_MIMIC)
                this.draw_icon(icons.MIMIC, x, y);

            if (fg & enums.TILE_FLAG_BERSERK)
            {
                this.draw_icon(icons.BERSERK, x, y);
                status_shift += 10;
            }

            // Pet mark
            if (fg & enums.TILE_FLAG_ATT_MASK)
            {
                att_flag = fg & enums.TILE_FLAG_ATT_MASK;
                if (att_flag == enums.TILE_FLAG_PET)
                {
                    this.draw_icon(icons.HEART, x, y);
                    status_shift += 10;
                }
                else if (att_flag == enums.TILE_FLAG_GD_NEUTRAL)
                {
                    this.draw_icon(icons.GOOD_NEUTRAL, x, y);
                    status_shift += 8;
                }
                else if (att_flag == enums.TILE_FLAG_NEUTRAL)
                {
                    this.draw_icon(icons.NEUTRAL, x, y);
                    status_shift += 8;
                }
            }

            if (fg & enums.TILE_FLAG_BEH_MASK)
            {
                var beh_flag = fg & enums.TILE_FLAG_BEH_MASK;
                if (beh_flag == enums.TILE_FLAG_STAB)
                {
                    this.draw_icon(icons.STAB_BRAND, x, y);
                    status_shift += 15;
                }
                else if (beh_flag == enums.TILE_FLAG_MAY_STAB)
                {
                    this.draw_icon(icons.MAY_STAB_BRAND, x, y);
                    status_shift += 8;
                }
                else if (beh_flag == enums.TILE_FLAG_FLEEING)
                {
                    this.draw_icon(icons.FLEEING, x, y);
                    status_shift += 4;
                }
            }

            if (fg & enums.TILE_FLAG_POISON)
            {
                this.draw_icon(icons.POISON, x, y, -status_shift, 0);
                status_shift += 5;
            }
            if (fg & enums.TILE_FLAG_STICKY_FLAME)
            {
                this.draw_icon(icons.STICKY_FLAME, x, y, -status_shift, 0);
                status_shift += 5;
            }
            if (fg & enums.TILE_FLAG_INNER_FLAME)
            {
                this.draw_icon(icons.INNER_FLAME, x, y, -status_shift, 0);
                status_shift += 8;
            }
            if (fg & enums.TILE_FLAG_CONSTRICTED)
            {
                this.draw_icon(icons.CONSTRICTED, x, y, -status_shift, 0);
                status_shift += 13;
            }

            if (fg & enums.TILE_FLAG_ANIM_WEP)
                this.draw_icon(icons.ANIMATED_WEAPON, x, y);

            if (bg & enums.TILE_FLAG_UNSEEN && ((bg != enums.TILE_FLAG_UNSEEN) || fg))
                this.draw_icon(icons.MESH, x, y);

            if (bg & enums.TILE_FLAG_OOR && ((bg != enums.TILE_FLAG_OOR) || fg))
                this.draw_icon(icons.OOR_MESH, x, y);

            if (bg & enums.TILE_FLAG_MM_UNSEEN && ((bg != enums.TILE_FLAG_MM_UNSEEN) || fg))
                this.draw_icon(icons.MAGIC_MAP_MESH, x, y);

            // Don't let the "new stair" icon cover up any existing icons, but
            // draw it otherwise.
            if (bg & enums.TILE_FLAG_NEW_STAIR && status_shift == 0)
                this.draw_icon(icons.NEW_STAIR, x, y);

            if (bg & enums.TILE_FLAG_EXCL_CTR && (bg & enums.TILE_FLAG_UNSEEN))
                this.draw_icon(icons.TRAVEL_EXCLUSION_CENTRE_FG, x, y);
            else if (bg & enums.TILE_FLAG_TRAV_EXCL && (bg & enums.TILE_FLAG_UNSEEN))
                this.draw_icon(icons.TRAVEL_EXCLUSION_FG, x, y);

            // Tutorial cursor takes precedence over other cursors.
            if (bg & enums.TILE_FLAG_TUT_CURSOR)
            {
                this.draw_icon(icons.TUTORIAL_CURSOR, x, y);
            }
            else if (bg & enums.TILE_FLAG_CURSOR)
            {
                var type = ((bg & enums.TILE_FLAG_CURSOR) == enums.TILE_FLAG_CURSOR1) ?
                    icons.CURSOR : icons.CURSOR2;

                if ((bg & enums.TILE_FLAG_CURSOR) == enums.TILE_FLAG_CURSOR3)
                    type = icons.CURSOR3;

                this.draw_icon(type, x, y);
            }

            if (fg & enums.TILE_FLAG_MDAM_MASK)
            {
                var mdam_flag = fg & enums.TILE_FLAG_MDAM_MASK;
                if (mdam_flag == enums.TILE_FLAG_MDAM_LIGHT)
                    this.draw_icon(icons.MDAM_LIGHTLY_DAMAGED, x, y);
                else if (mdam_flag == enums.TILE_FLAG_MDAM_MOD)
                    this.draw_icon(icons.MDAM_MODERATELY_DAMAGED, x, y);
                else if (mdam_flag == enums.TILE_FLAG_MDAM_HEAVY)
                    this.draw_icon(icons.MDAM_HEAVILY_DAMAGED, x, y);
                else if (mdam_flag == enums.TILE_FLAG_MDAM_SEV)
                    this.draw_icon(icons.MDAM_SEVERELY_DAMAGED, x, y);
                else if (mdam_flag == enums.TILE_FLAG_MDAM_ADEAD)
                    this.draw_icon(icons.MDAM_ALMOST_DEAD, x, y);
            }

            if (fg & enums.TILE_FLAG_DEMON)
            {
                var demon_flag = fg & enums.TILE_FLAG_DEMON;
                if (demon_flag == enums.TILE_FLAG_DEMON_1)
                    this.draw_icon(icons.DEMON_NUM1, x, y);
                else if (demon_flag == enums.TILE_FLAG_DEMON_2)
                    this.draw_icon(icons.DEMON_NUM2, x, y);
                else if (demon_flag == enums.TILE_FLAG_DEMON_3)
                    this.draw_icon(icons.DEMON_NUM3, x, y);
                else if (demon_flag == enums.TILE_FLAG_DEMON_4)
                    this.draw_icon(icons.DEMON_NUM4, x, y);
                else if (demon_flag == enums.TILE_FLAG_DEMON_5)
                    this.draw_icon(icons.DEMON_NUM5, x, y);
            }
        },


        // Helper functions for drawing from specific textures
        draw_tile: function(idx, x, y, mod, ofsx, ofsy, y_max)
        {
            var info = mod.get_tile_info(idx);
            var img = get_img(mod.get_img(idx));
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
            this.draw_tile(idx, x, y, dngn);
        },

        draw_main: function(idx, x, y)
        {
            this.draw_tile(idx, x, y, main);
        },

        draw_player: function(idx, x, y, ofsx, ofsy, y_max)
        {
            this.draw_tile(idx, x, y, player, ofsx, ofsy, y_max);
        },

        draw_icon: function(idx, x, y, ofsx, ofsy)
        {
            this.draw_tile(idx, x, y, icons, ofsx, ofsy);
        },

        draw_from_texture: function (idx, x, y, tex, ofsx, ofsy, y_max)
        {
            var mod = tileinfos(tex);
            this.draw_tile(idx, x, y, mod, ofsx, ofsy);
        },
    });

    return {
        DungeonCellRenderer: DungeonCellRenderer,
    };
});
