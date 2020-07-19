define(["jquery", "./view_data", "./tileinfo-main", "./tileinfo-player",
        "./tileinfo-icons", "./tileinfo-dngn", "./enums", "./map_knowledge",
        "./tileinfos", "./player", "./options", "contrib/jquery.json"],
function ($, view_data, main, tileinfo_player, icons, dngn, enums,
          map_knowledge, tileinfos, player, options) {
    "use strict";

    function DungeonCellRenderer()
    {
        // see also, renderer_settings in game.js. In fact, do these values
        // do anything?
        var ratio = window.devicePixelRatio;
        this.set_cell_size(32, 32);
        this.glyph_mode_font_size = 24 * ratio;
        this.glyph_mode_font = "monospace";
    }

    var fg_term_colours, bg_term_colours;
    var healthy, hp_spend, magic, magic_spend;

    function determine_colours()
    {
        fg_term_colours = [];
        bg_term_colours = [];
        var $game = $("#game");
        for (var i = 0; i < 16; ++i)
        {
            var elem = $("<span class='glyph fg" + i + " bg" + i + "'></span>");
            $game.append(elem);
            fg_term_colours.push(elem.css("color"));
            bg_term_colours.push(elem.css("background-color"));
            elem.detach();
        }

        // FIXME: CSS lookup doesn't work on Chrome after saving and loading a
        // game, style information is missing for some reason.
        // Use hard coded values instead.
        healthy = "#8ae234";
        hp_spend = "#a40000";
        magic = "#729fcf";
        // healthy = $("#stats_hp_bar_full").css("background-color");
        // hp_spend = $("#stats_hp_bar_decrease").css("background-color");
        // magic = $("#stats_mp_bar_full").css("background-color");
        magic_spend = "black";
    }

    function in_water(cell)
    {
        return ((cell.bg.WATER) && !(cell.fg.FLYING));
    }

    function split_term_colour(col)
    {
        var fg = col & 0xF;
        var bg = 0;
        var attr = (col & 0xF0) >> 4;
        var param = (col & 0xF000) >> 12;
        return { fg: fg, bg: bg, attr: attr, param: param };
    }

    function term_colour_apply_attributes(col)
    {
        if (col.attr == enums.CHATTR.HILITE)
        {
            col.bg = col.param;
            if (col.bg == col.fg)
                col.fg = 0;
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
        return $.toJSON(o);
    }

    $.extend(DungeonCellRenderer.prototype, {
        init: function(element)
        {
            this.element = element;
            this.ctx = this.element.getContext("2d");
        },

        set_cell_size: function(w, h)
        {
            this.cell_width = Math.floor(w);
            this.cell_height = Math.floor(h);
            this.x_scale = this.cell_width / 32;
            this.y_scale = this.cell_height / 32;
        },

        glyph_mode_font_name: function ()
        {
            var glyph_scale;
            if (this.ui_state == enums.ui.VIEW_MAP)
                glyph_scale = options.get("tile_map_scale");
            else
                glyph_scale = options.get("tile_viewport_scale");

            return (Math.floor(this.glyph_mode_font_size * glyph_scale / 100)
                + "px " + this.glyph_mode_font);
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
                if (options.get("tile_display_mode") != "glyphs")
                    this.render_flash(x, y);

                this.render_cursors(cx, cy, x, y);
                return;
            }

            // track whether this cell overlaps to the top or left
            this.current_sy = 0;
            this.current_left_overlap = 0;

            cell.fg = enums.prepare_fg_flags(cell.fg || 0);
            cell.bg = enums.prepare_bg_flags(cell.bg || 0);
            cell.cloud = enums.prepare_fg_flags(cell.cloud || 0);
            cell.flv = cell.flv || {};
            cell.flv.f = cell.flv.f || 0;
            cell.flv.s = cell.flv.s || 0;
            map_cell.g = map_cell.g || ' ';
            if (map_cell.col == undefined) map_cell.col = 7;

            if (options.get("tile_display_mode") == "glyphs")
            {
                this.render_glyph(x, y, map_cell);

                this.render_cursors(cx, cy, x, y);
                this.draw_ray(x, y, cell);
                return;
            }

            // cell is basically a packed_cell + doll + mcache entries
            if (options.get("tile_display_mode") == "tiles")
                this.draw_background(x, y, cell);

            var fg_idx = cell.fg.value;
            var is_in_water = in_water(cell);

            // draw clouds
            if (cell.cloud.value)
            {
                this.ctx.save();
                // If there will be a front/back cloud pair, draw
                // the underlying one with correct alpha
                if (fg_idx)
                {
                    try
                    {
                        this.ctx.globalAlpha = 0.6;
                        this.set_nonsubmerged_clip(x, y, 20);
                        this.draw_main(cell.cloud.value, x, y);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }

                    this.ctx.save();
                    try
                    {
                        this.ctx.globalAlpha = 0.2;
                        this.set_submerged_clip(x, y, 20);
                        this.draw_main(cell.cloud.value, x, y);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }
                else
                {
                    try
                    {
                        this.ctx.globalAlpha = 1.0;
                        this.draw_main(cell.cloud.value, x, y);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }
            }

            // Canvas doesn't support applying an alpha gradient
            // to an image while drawing; so to achieve the same
            // effect as in local tiles, it would probably be best
            // to pregenerate water tiles with the (inverse) alpha
            // gradient built in. This simply draws the lower
            // half with increased transparency; for now, it looks
            // good enough.

            var renderer = this;
            function draw_dolls()
            {
                if ((fg_idx >= main.MAIN_MAX) && cell.doll)
                {
                    var mcache_map = {};
                    if (cell.mcache)
                    {
                        for (var i = 0; i < cell.mcache.length; ++i)
                            mcache_map[cell.mcache[i][0]] = i;
                    }
                    $.each(cell.doll, function (i, doll_part) {
                        var xofs = 0;
                        var yofs = 0;
                        if (mcache_map[doll_part[0]] !== undefined)
                        {
                            var mind = mcache_map[doll_part[0]];
                            xofs = cell.mcache[mind][1];
                            yofs = cell.mcache[mind][2];
                        }
                        renderer.draw_player(doll_part[0],
                                             x, y, xofs, yofs, doll_part[1]);
                    });
                }

                if ((fg_idx >= tileinfo_player.MCACHE_START) && cell.mcache)
                {
                    $.each(cell.mcache, function (i, mcache_part) {
                        if (mcache_part) {
                            renderer.draw_player(mcache_part[0],
                                                 x, y, mcache_part[1], mcache_part[2]);
                        }
                    });
                }
            }

            if (is_in_water && options.get("tile_display_mode") == "tiles")
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
            else if (options.get("tile_display_mode") == "tiles")
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

            // draw clouds over stuff
            if (fg_idx && cell.cloud.value)
            {
                this.ctx.save();
                try
                {
                    this.ctx.globalAlpha = 0.4;
                    this.set_nonsubmerged_clip(x, y, 20);
                    this.draw_main(cell.cloud.value, x, y);
                }
                finally
                {
                    this.ctx.restore();
                }

                this.ctx.save();
                try
                {
                    this.ctx.globalAlpha = 0.8;
                    this.set_submerged_clip(x, y, 20);
                    this.draw_main(cell.cloud.value, x, y);
                }
                finally
                {
                    this.ctx.restore();
                }
            }

            // Draw main-tile overlays (i.e. zaps), on top of clouds.
            if (cell.ov)
            {
                $.each(cell.ov, function (i, overlay)
                        {
                            if (dngn.FEAT_MAX <= overlay && overlay < main.MAIN_MAX)
                            {
                                renderer.draw_main(overlay, x, y);
                            }
                        });
            }

            this.render_flash(x, y);

            this.render_cursors(cx, cy, x, y);

            if (cx == player.pos.x && cy == player.pos.y
                && map_knowledge.player_on_level())
            {
                this.draw_minibars(x, y);
            }

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
            cell.left_overlap = this.current_left_overlap;
        },

        // adapted from DungeonRegion::draw_minibars in tilereg_dgn.cc
        draw_minibars: function(x, y)
        {
            var show_health = options.get("tile_show_minihealthbar");
            var show_magic = options.get("tile_show_minimagicbar");

            // don't draw if hp and mp is full
            if ((player.hp == player.hp_max || !show_health) &&
                (player.mp == player.mp_max || !show_magic))
                return;

            var bar_height = Math.floor(this.cell_height/16);
            var hp_bar_offset = bar_height;

            // TODO: use different colors if heavily wounded, like in the tiles version
            if (player.mp_max > 0 && show_magic)
            {
                var mp_percent = player.mp / player.mp_max;
                if (mp_percent < 0) mp_percent = 0;

                this.ctx.fillStyle = magic_spend;
                this.ctx.fillRect(x, y + this.cell_height - bar_height,
                                  this.cell_width, bar_height);

                this.ctx.fillStyle = magic;
                this.ctx.fillRect(x, y + this.cell_height - bar_height,
                                  this.cell_width * mp_percent, bar_height);

                hp_bar_offset += bar_height;
            }

            if (show_health)
            {
                var hp_percent = player.hp / player.hp_max;
                if (hp_percent < 0) hp_percent = 0;

                this.ctx.fillStyle = hp_spend;
                this.ctx.fillRect(x, y + this.cell_height - hp_bar_offset,
                                  this.cell_width, bar_height);

                this.ctx.fillStyle = healthy;
                this.ctx.fillRect(x, y + this.cell_height - hp_bar_offset,
                                  this.cell_width * hp_percent, bar_height);
            }
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
                this.ctx.fillStyle = bg_term_colours[col.bg];
                this.ctx.fillRect(x, y, this.cell_width, this.cell_height);
            }
            this.ctx.fillStyle = fg_term_colours[col.fg];
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
            var offset;

            if (cell.liquefied && !is_wall)
            {
                offset = cell.flv.s % dngn.tile_count(dngn.LIQUEFACTION);
                this.draw_dngn(dngn.LIQUEFACTION + offset, x, y);
            }
            else if (cell.bloody)
            {
                cell.blood_rotation = cell.blood_rotation || 0;
                var basetile;
                if (is_wall)
                {
                    basetile = cell.old_blood ? dngn.WALL_OLD_BLOOD : dngn.WALL_BLOOD_S;
                    basetile += dngn.tile_count(basetile) * cell.blood_rotation;
                    basetile = dngn.WALL_BLOOD_S + dngn.tile_count(dngn.WALL_BLOOD_S)
                        * cell.blood_rotation;
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

        draw_ray: function(x, y, cell)
        {
            var bg = cell.bg;
            var bg_idx = cell.bg.value;
            var renderer = this;

            if (bg.RAY)
                this.draw_dngn(dngn.RAY, x, y);
            else if (bg.RAY_OOR)
                this.draw_dngn(dngn.RAY_OUT_OF_RANGE, x, y);
            else if (bg.LANDING)
                this.draw_dngn(dngn.LANDING, x, y);
            else if (bg.RAY_MULTI)
                this.draw_dngn(dngn.RAY_MULTI, x, y);
        },

        draw_background: function(x, y, cell)
        {
            var bg = cell.bg;
            var bg_idx = cell.bg.value;
            var renderer = this;

            if (cell.mangrove_water && bg_idx > dngn.DNGN_UNSEEN)
                this.draw_dngn(dngn.DNGN_SHALLOW_WATER, x, y);
            else if (bg_idx >= dngn.DNGN_FIRST_TRANSPARENT)
            {
                this.draw_dngn(cell.flv.f, x, y); // f = floor

                // Draw floor overlays beneath the feature
                if (cell.ov)
                {
                    $.each(cell.ov, function (i, overlay)
                           {
                               if (overlay && overlay <= dngn.FLOOR_MAX)
                                   renderer.draw_dngn(overlay, x, y);
                           });
                }
            }

            // Draw blood beneath feature tiles.
            if (bg_idx > dngn.WALL_MAX)
                this.draw_blood_overlay(x, y, cell);

            if (cell.mangrove_water) // Draw the tree submerged
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
                // Draw blood on top of wall tiles.
                if (bg_idx <= dngn.WALL_MAX)
                    this.draw_blood_overlay(x, y, cell, bg_idx > dngn.FLOOR_MAX);

                // Draw overlays
                if (cell.ov)
                {
                    $.each(cell.ov, function (i, overlay)
                           {
                               if (overlay > dngn.DNGN_MAX)
                                   return;
                               else if (overlay &&
                                   (bg_idx < dngn.DNGN_FIRST_TRANSPARENT ||
                                    overlay > dngn.FLOOR_MAX))
                               {
                                   renderer.draw_dngn(overlay, x, y);
                               }
                           });
                }

                if (!bg.UNSEEN)
                {
                    if (bg.KRAKEN_NW)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_NW, x, y);
                    else if (bg.ELDRITCH_NW)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_NW, x, y);
                    if (bg.KRAKEN_NE)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_NE, x, y);
                    else if (bg.ELDRITCH_NE)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_NE, x, y);
                    if (bg.KRAKEN_SE)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_SE, x, y);
                    else if (bg.ELDRITCH_SE)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_SE, x, y);
                    if (bg.KRAKEN_SW)
                        this.draw_dngn(dngn.KRAKEN_OVERLAY_SW, x, y);
                    else if (bg.ELDRITCH_SW)
                        this.draw_dngn(dngn.ELDRITCH_OVERLAY_SW, x, y);
                }

                if (!bg.UNSEEN)
                {
                    if (cell.sanctuary)
                        this.draw_dngn(dngn.SANCTUARY, x, y);
                    // TAG_MAJOR_VERSION == 34
                    if (cell.heat_aura)
                        this.draw_dngn(dngn.HEAT_AURA + cell.heat_aura - 1, x, y);
                    if (cell.silenced)
                        this.draw_dngn(dngn.SILENCED, x, y);
                    if (cell.halo == enums.HALO_RANGE)
                        this.draw_dngn(dngn.HALO_RANGE, x, y);
                    if (cell.halo == enums.HALO_UMBRA)
                        this.draw_dngn(dngn.UMBRA, x, y);
                    if (cell.orb_glow)
                        this.draw_dngn(dngn.ORB_GLOW + cell.orb_glow - 1, x, y);
                    if (cell.quad_glow)
                        this.draw_dngn(dngn.QUAD_GLOW, x, y);
                    if (cell.disjunct)
                        this.draw_dngn(dngn.DISJUNCT + cell.disjunct - 1, x, y);
                    if (cell.awakened_forest)
                        this.draw_icon(icons.BERSERK, x, y);

                    if (cell.fg)
                    {
                        var fg = cell.fg;
                        if (fg.PET)
                            this.draw_dngn(dngn.HALO_FRIENDLY, x, y);
                        else if (fg.GD_NEUTRAL)
                            this.draw_dngn(dngn.HALO_GD_NEUTRAL, x, y);
                        else if (fg.NEUTRAL)
                            this.draw_dngn(dngn.HALO_NEUTRAL, x, y);

                        // Monster difficulty
                        if (fg.TRIVIAL)
                            this.draw_dngn(dngn.THREAT_TRIVIAL, x, y);
                        else if (fg.EASY)
                            this.draw_dngn(dngn.THREAT_EASY, x, y);
                        else if (fg.TOUGH)
                            this.draw_dngn(dngn.THREAT_TOUGH, x, y);
                        else if (fg.NASTY)
                            this.draw_dngn(dngn.THREAT_NASTY, x, y);

                        if (cell.highlighted_summoner)
                            this.draw_dngn(dngn.HALO_SUMMONER, x, y);
                    }


                    // Apply the travel exclusion under the foreground if the cell is
                    // visible. It will be applied later if the cell is unseen.
                    if (bg.EXCL_CTR)
                        this.draw_dngn(dngn.TRAVEL_EXCLUSION_CENTRE_BG, x, y);
                    else if (bg.TRAV_EXCL)
                        this.draw_dngn(dngn.TRAVEL_EXCLUSION_BG, x, y);
                }
            }

            this.draw_ray(x, y, cell);
        },

        draw_foreground: function(x, y, map_cell)
        {
            var cell = map_cell.t;
            var fg = cell.fg;
            var bg = cell.bg;
            var fg_idx = cell.fg.value;
            var is_in_water = in_water(cell);

            if (fg_idx && fg_idx <= main.MAIN_MAX && options.get("tile_display_mode") == "tiles")
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
            else if (options.get("tile_display_mode") == "hybrid")
            {
                this.render_glyph(x, y, map_cell, true);
                this.draw_ray(x, y, cell);
            }

            if (fg.NET)
                this.draw_icon(icons.TRAP_NET, x, y);

            if (fg.WEB)
                this.draw_icon(icons.TRAP_WEB, x, y);

            if (fg.S_UNDER)
                this.draw_icon(icons.SOMETHING_UNDER, x, y);

            if (fg.MIMIC_INEPT)
                this.draw_icon(icons.INEPT_MIMIC, x, y);
            else if (fg.MIMIC)
                this.draw_icon(icons.MIMIC, x, y);
            else if (fg.MIMIC_RAVEN)
                this.draw_icon(icons.RAVENOUS_MIMIC, x, y);

            // Pet mark
            if (fg.PET)
                this.draw_icon(icons.FRIENDLY, x, y);
            else if (fg.GD_NEUTRAL)
                this.draw_icon(icons.GOOD_NEUTRAL, x, y);
            else if (fg.NEUTRAL)
                this.draw_icon(icons.NEUTRAL, x, y);

            //These icons are in the lower right, so status_shift doesn't need changing.
            if (fg.BERSERK)
                this.draw_icon(icons.BERSERK, x, y);
            if (fg.IDEALISED)
                this.draw_icon(icons.IDEALISED, x, y);


            var status_shift = 0;
            if (fg.STAB)
            {
                this.draw_icon(icons.STAB_BRAND, x, y);
                status_shift += 12;
            }
            else if (fg.MAY_STAB)
            {
                this.draw_icon(icons.MAY_STAB_BRAND, x, y);
                status_shift += 7;
            }
            else if (fg.FLEEING)
            {
                this.draw_icon(icons.FLEEING, x, y);
                status_shift += 3;
            }

            if (fg.POISON)
            {
                this.draw_icon(icons.POISON, x, y, -status_shift, 0);
                status_shift += 5;
            }
            else if (fg.MORE_POISON)
            {
                this.draw_icon(icons.MORE_POISON, x, y, -status_shift, 0);
                status_shift += 5;
            }
            else if (fg.MAX_POISON)
            {
                this.draw_icon(icons.MAX_POISON, x, y, -status_shift, 0);
                status_shift += 5;
            }

            if (fg.STICKY_FLAME)
            {
                this.draw_icon(icons.STICKY_FLAME, x, y, -status_shift, 0);
                status_shift += 7;
            }
            if (fg.INNER_FLAME)
            {
                this.draw_icon(icons.INNER_FLAME, x, y, -status_shift, 0);
                status_shift += 7;
            }
            if (fg.CONSTRICTED)
            {
                this.draw_icon(icons.CONSTRICTED, x, y, -status_shift, 0);
                status_shift += 11;
            }
            if (fg.VILE_CLUTCH)
            {
                this.draw_icon(icons.VILE_CLUTCH, x, y, -status_shift, 0);
                status_shift += 11;
            }
            if (fg.POSSESSABLE)
            {
                this.draw_icon(icons.POSSESSABLE, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.GLOWING)
            {
                this.draw_icon(icons.GLOWING, x, y, -status_shift, 0);
                status_shift += 8;
            }
            if (fg.SWIFT)
            {
                this.draw_icon(icons.SWIFT, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.HASTED)
            {
                this.draw_icon(icons.HASTED, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.SLOWED)
            {
                this.draw_icon(icons.SLOWED, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.MIGHT)
            {
                this.draw_icon(icons.MIGHT, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.CORRODED)
            {
                this.draw_icon(icons.CORRODED, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.DRAIN)
            {
                this.draw_icon(icons.DRAIN, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.PAIN_MIRROR)
            {
                this.draw_icon(icons.PAIN_MIRROR, x, y, -status_shift, 0);
                status_shift += 7;
            }
            if (fg.PETRIFYING)
            {
                this.draw_icon(icons.PETRIFYING, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.PETRIFIED)
            {
                this.draw_icon(icons.PETRIFIED, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.BLIND)
            {
                this.draw_icon(icons.BLIND, x, y, -status_shift, 0);
                status_shift += 10;
            }
            if (fg.DEATHS_DOOR)
            {
                this.draw_icon(icons.DEATHS_DOOR, x, y, -status_shift, 0);
                status_shift += 10;
            }
            if (fg.BOUND_SOUL)
            {
                this.draw_icon(icons.BOUND_SOUL, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.INFESTED)
            {
                this.draw_icon(icons.INFESTED, x, y, -status_shift, 0);
                status_shift += 6;
            }
            if (fg.RECALL)
            {
                this.draw_icon(icons.RECALL, x, y, -status_shift, 0);
                status_shift += 9;
            }

            // Anim. weap. and summoned might overlap, but that's okay
            if (fg.ANIM_WEP)
                this.draw_icon(icons.ANIMATED_WEAPON, x, y);
            if (fg.SUMMONED)
                this.draw_icon(icons.SUMMONED, x, y);
            if (fg.PERM_SUMMON)
                this.draw_icon(icons.PERM_SUMMON, x, y);

            if (bg.UNSEEN && (bg.value || fg.value))
                this.draw_icon(icons.MESH, x, y);

            if (bg.OOR && (bg.value || fg.value))
                this.draw_icon(icons.OOR_MESH, x, y);

            if (bg.MM_UNSEEN && (bg.value || fg.value))
                this.draw_icon(icons.MAGIC_MAP_MESH, x, y);

            // Don't let the "new stair" icon cover up any existing icons, but
            // draw it otherwise.
            if (bg.NEW_STAIR && status_shift == 0)
                this.draw_icon(icons.NEW_STAIR, x, y);

            if (bg.NEW_TRANSPORTER && status_shift == 0)
                this.draw_icon(icons.NEW_TRANSPORTER, x, y);

            if (bg.EXCL_CTR && bg.UNSEEN)
                this.draw_icon(icons.TRAVEL_EXCLUSION_CENTRE_FG, x, y);
            else if (bg.TRAV_EXCL && bg.UNSEEN)
                this.draw_icon(icons.TRAVEL_EXCLUSION_FG, x, y);

            // Tutorial cursor takes precedence over other cursors.
            if (bg.TUT_CURSOR)
            {
                this.draw_icon(icons.TUTORIAL_CURSOR, x, y);
            }
            else if (bg.CURSOR1)
            {
                this.draw_icon(icons.CURSOR, x, y);
            }
            else if (bg.CURSOR2)
            {
                this.draw_icon(icons.CURSOR2, x, y);
            }
            else if (bg.CURSOR3)
            {
                this.draw_icon(icons.CURSOR3, x, y);
            }

            if (cell.travel_trail & 0xF)
            {
                this.draw_icon(icons.TRAVEL_PATH_FROM +
                               (cell.travel_trail & 0xF) - 1, x, y);
            }
            if (cell.travel_trail & 0xF0)
            {
                this.draw_icon(icons.TRAVEL_PATH_TO +
                               ((cell.travel_trail & 0xF0) >> 4) - 1, x, y);
            }

            if (fg.MDAM_LIGHT)
                this.draw_icon(icons.MDAM_LIGHTLY_DAMAGED, x, y);
            else if (fg.MDAM_MOD)
                this.draw_icon(icons.MDAM_MODERATELY_DAMAGED, x, y);
            else if (fg.MDAM_HEAVY)
                this.draw_icon(icons.MDAM_HEAVILY_DAMAGED, x, y);
            else if (fg.MDAM_SEV)
                this.draw_icon(icons.MDAM_SEVERELY_DAMAGED, x, y);
            else if (fg.MDAM_ADEAD)
                this.draw_icon(icons.MDAM_ALMOST_DEAD, x, y);

            if (options.get("tile_show_demon_tier") === true)
            {
                if (fg.DEMON_1)
                    this.draw_icon(icons.DEMON_NUM1, x, y);
                else if (fg.DEMON_2)
                    this.draw_icon(icons.DEMON_NUM2, x, y);
                else if (fg.DEMON_3)
                    this.draw_icon(icons.DEMON_NUM3, x, y);
                else if (fg.DEMON_4)
                    this.draw_icon(icons.DEMON_NUM4, x, y);
                else if (fg.DEMON_5)
                    this.draw_icon(icons.DEMON_NUM5, x, y);
            }
        },


        // Helper functions for drawing from specific textures
        draw_tile: function(idx, x, y, mod, ofsx, ofsy, y_max, centre)
        {
            var info = mod.get_tile_info(idx);
            var img = get_img(mod.get_img(idx));
            if (!info)
            {
                throw ("Tile not found: " + idx);
            }
            centre = centre === undefined ? true : centre;
            var size_ox = !centre ? 0 : 32 / 2 - info.w / 2;
            var size_oy = !centre ? 0 : 32 - info.h;
            var pos_sy_adjust = (ofsy || 0) + info.oy + size_oy;
            var pos_ey_adjust = pos_sy_adjust + info.ey - info.sy;
            var sy = pos_sy_adjust;
            var ey = pos_ey_adjust;
            if (y_max && y_max < ey)
                ey = y_max;

            if (sy >= ey) return;

            var total_x_offset = ((ofsx || 0) + info.ox + size_ox);

            if (total_x_offset < this.current_left_overlap)
                this.current_left_overlap = total_x_offset;

            if (sy < this.current_sy)
                this.current_sy = sy;

            var w = info.ex - info.sx;
            var h = info.ey - info.sy;

            this.ctx.imageSmoothingEnabled = options.get("tile_filter_scaling");
            this.ctx.webkitImageSmoothingEnabled =
                options.get("tile_filter_scaling");
            this.ctx.mozImageSmoothingEnabled =
                options.get("tile_filter_scaling");
            this.ctx.drawImage(img,
                               info.sx, info.sy + sy - pos_sy_adjust,
                               w, h + ey - pos_ey_adjust,
                               x + total_x_offset * this.x_scale,
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
            this.draw_tile(idx, x, y, tileinfo_player, ofsx, ofsy, y_max);
        },

        draw_icon: function(idx, x, y, ofsx, ofsy)
        {
            this.draw_tile(idx, x, y, icons, ofsx, ofsy);
        },

        draw_from_texture: function (idx, x, y, tex, ofsx, ofsy, y_max, centre)
        {
            var mod = tileinfos(tex);
            this.draw_tile(idx, x, y, mod, ofsx, ofsy, y_max, centre);
        },
    });

    $(document).off("game_init.cell_renderer")
        .on("game_init.cell_renderer", function () {
            determine_colours();
        });

    return {
        DungeonCellRenderer: DungeonCellRenderer,
    };
});
