define(["jquery", "./view_data", "./tileinfo-gui", "./tileinfo-main",
        "./tileinfo-player", "./tileinfo-icons", "./tileinfo-dngn", "./enums",
        "./map_knowledge", "./tileinfos", "./player", "./options",
        "contrib/jquery.json"],
function ($, view_data, gui, main, tileinfo_player, icons, dngn, enums,
          map_knowledge, tileinfos, player, options) {
    "use strict";

    function DungeonCellRenderer()
    {
        this.set_cell_size(32, 32);
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
        hp_spend = "#b30009";
        magic = "#5e78ff";
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

        glyph_mode_font_name: function (scale, device)
        {
            var glyph_scale;
            if (scale)
                glyph_scale = scale * 100;
            else
            {
                if (this.ui_state == enums.ui.VIEW_MAP)
                    glyph_scale = options.get("tile_map_scale");
                else
                    glyph_scale = options.get("tile_viewport_scale");
                glyph_scale = ((glyph_scale - 100) / 2 + 100);
            }
            if (device)
                glyph_scale = glyph_scale * window.devicePixelRatio;

            return (Math.floor(this.glyph_mode_font_size * glyph_scale / 100)
                + "px " + this.glyph_mode_font);
        },

        glyph_mode_update_font_metrics: function ()
        {
            this.ctx.font = this.glyph_mode_font_name();

            // Glyph used here does not matter because fontBoundingBoxAscent
            // and fontBoundingBoxDescent are specific to the font whereas all
            // glyphs in a monospaced font will have the same width
            var metrics = this.ctx.measureText('@');
            this.glyph_mode_font_width = Math.ceil(metrics.width);

            // 2022: this feature appears to still be unavailable by default
            // in firefox
            if (metrics.fontBoundingBoxAscent)
            {
                this.glyph_mode_baseline = metrics.fontBoundingBoxAscent;
                this.glyph_mode_line_height = (metrics.fontBoundingBoxAscent
                                            + metrics.fontBoundingBoxDescent);
            }
            else
            {   // Inspired by https://stackoverflow.com/q/1134586/
                var body = document.body;
                var ref_glyph = document.createElement("span");
                var ref_block = document.createElement("div");
                var div = document.createElement("div");

                ref_glyph.innerHTML = '@';
                ref_glyph.style.font = this.ctx.font;

                ref_block.style.display = "inline-block";
                ref_block.style.width = "1px";
                ref_block.style.height = "0px";

                div.style.visibility = "hidden";
                div.appendChild(ref_glyph);
                div.appendChild(ref_block);
                body.appendChild(div);

                try
                {
                    ref_block.style["vertical-align"] = "baseline";
                    this.glyph_mode_baseline = (ref_block.offsetTop
                                                - ref_glyph.offsetTop);
                    ref_block.style["vertical-align"] = "bottom";
                    this.glyph_mode_line_height = (ref_block.offsetTop
                                                    - ref_glyph.offsetTop);
                }
                finally
                {
                    document.body.removeChild(div);
                }
            }
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

        scaled_size: function(w, h)
        {
            w = w === undefined ? this.cell_width : w;
            h = h === undefined ? this.cell_height : h;
            const ratio = window.devicePixelRatio;
            const width = Math.floor(w * ratio);
            const height = Math.floor(h * ratio);
            return {width: width, height: height};
        },

        clear: function()
        {
            this.ctx.fillStyle = "black";
            // element dimensions are already scaled
            this.ctx.fillRect(0, 0, this.element.width, this.element.height);
        },

        do_render_cell: function(cx, cy, x, y, map_cell, cell)
        {
            let scaled = this.scaled_size();

            this.ctx.fillStyle = "black";
            this.ctx.fillRect(x, y, scaled.width, scaled.height);

            map_cell = map_cell || map_knowledge.get(cx, cy);
            cell = cell || map_cell.t;

            if (!cell)
            {
                this.render_cursors(cx, cy, x, y);
                return;
            }

            // track whether this cell overlaps to the top or left
            this.current_sy = 0;
            this.current_left_overlap = 0;

            cell.fg = enums.prepare_fg_flags(cell.fg || 0);
            cell.bg = enums.prepare_bg_flags(cell.bg || 0);
            cell.cloud = enums.prepare_fg_flags(cell.cloud || 0);
            cell.icons = cell.icons || [];
            cell.flv = cell.flv || {};
            cell.flv.f = cell.flv.f || 0;
            cell.flv.s = cell.flv.s || 0;
            map_cell.g = map_cell.g || ' ';
            if (map_cell.col == undefined) map_cell.col = 7;

            if (options.get("tile_display_mode") == "glyphs")
            {
                this.render_glyph(x, y, map_cell, false);

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
                    this.ctx.globalAlpha = cell.trans ? 0.55 : 1.0;

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

            this.render_flash(x, y, map_cell);

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
                let scaled = this.scaled_size();
                this.ctx.fillText(cell.mark,
                                  x + 0.5 * scaled.width,
                                  y + 0.5 * scaled.height);
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

            let cell = this.scaled_size();
            var bar_height = Math.floor(cell.height / 16);
            var hp_bar_offset = bar_height;

            // TODO: use different colors if heavily wounded, like in the tiles version
            if (player.mp_max > 0 && show_magic)
            {
                var mp_percent = player.mp / player.mp_max;
                if (mp_percent < 0) mp_percent = 0;

                this.ctx.fillStyle = magic_spend;
                this.ctx.fillRect(x, y + cell.height - bar_height,
                                  cell.width, bar_height);

                this.ctx.fillStyle = magic;
                this.ctx.fillRect(x, y + cell.height - bar_height,
                                  cell.width * mp_percent, bar_height);

                hp_bar_offset += bar_height;
            }

            if (show_health)
            {
                var hp_percent = player.hp / player.hp_max;
                if (hp_percent < 0) hp_percent = 0;

                this.ctx.fillStyle = hp_spend;
                this.ctx.fillRect(x, y + cell.height - hp_bar_offset,
                                  cell.width, bar_height);

                this.ctx.fillStyle = healthy;
                this.ctx.fillRect(x, y + cell.height - hp_bar_offset,
                                  cell.width * hp_percent, bar_height);
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

        render_glyph: function (x, y, map_cell, omit_bg, square, scale)
        {
            // `map_cell` can be anything as long as it provides `col` and `g`
            var col = split_term_colour(map_cell.col);
            if (omit_bg && col.attr == enums.CHATTR.REVERSE)
                col.attr = 0;
            term_colour_apply_attributes(col);
            let cell = this.scaled_size();
            var w = cell.width;
            var h = cell.height;
            if (scale)
            {
                // assume x and y are already scaled...
                w = w * scale;
                h = h * scale;
            }

            var prefix = "";
            if (col.attr == enums.CHATTR.BOLD)
                prefix = "bold ";

            if (!omit_bg)
            {
                this.ctx.fillStyle = bg_term_colours[col.bg];
                this.ctx.fillRect(x, y, w, h);
            }
            this.ctx.fillStyle = fg_term_colours[col.fg];
            this.ctx.font = prefix + this.glyph_mode_font_name(scale, true);

            this.ctx.save();

            try
            {
                this.ctx.beginPath();
                this.ctx.rect(x, y, w, h);
                this.ctx.clip();

                if (square)
                {
                    this.ctx.textAlign = "center";
                    this.ctx.textBaseline = "middle";
                    this.ctx.fillText(map_cell.g,
                                        Math.floor(x + w/2),
                                        Math.floor(y + h/2));
                }
                else
                {
                    this.ctx.fillText(map_cell.g, x,
                        y + this.glyph_mode_baseline * window.devicePixelRatio);
                }
            }
            finally
            {
                this.ctx.restore();
            }
        },

        render_flash: function(x, y, map_cell)
        {
            if (map_cell.flc)
            {
                var col = view_data.get_flash_colour(map_cell.flc, map_cell.fla);
                this.ctx.save();
                try
                {
                    this.ctx.fillStyle = "rgb(" + col.r + "," +
                        col.g + "," + col.b + ")";
                    this.ctx.globalAlpha = col.a / 255;
                    const cell = this.scaled_size();
                    this.ctx.fillRect(x, y, cell.width, cell.height);
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
            const scaled = this.scaled_size(null, water_level * this.y_scale);
            // this clip is across the entire row
            this.ctx.rect(0, y + scaled.height,
                          this.element.width,
                          this.element.height - y - scaled.height);
            this.ctx.clip();
        },

        set_nonsubmerged_clip: function(x, y, water_level)
        {
            this.ctx.beginPath();
            const scaled = this.scaled_size(null, water_level * this.y_scale);
            this.ctx.rect(0, 0, this.element.width, y + scaled.height);
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
                var ray_tile = 0;
                if (cell.ov)
                {
                    $.each(cell.ov, function (i, overlay)
                        {
                            if (overlay > dngn.DNGN_MAX)
                                return;
                            else if (overlay == dngn.RAY
                                    || overlay == dngn.RAY_MULTI
                                    || overlay == dngn.RAY_OUT_OF_RANGE)
                            {
                                // these need to be drawn last because of the
                                // way alpha blending happens here, but for
                                // hard-to-change reasons they are assembled on
                                // the server side in the wrong order. (In local
                                // tiles it's ok to blend them in any order.)
                                // assumption: only one can appear on any tile.
                                // TODO: a more general fix for this issue?
                                ray_tile = overlay;
                            }
                            else if (overlay &&
                                (bg_idx < dngn.DNGN_FIRST_TRANSPARENT ||
                                 overlay > dngn.FLOOR_MAX))
                            {
                                renderer.draw_dngn(overlay, x, y);
                            }
                        });
                }
                if (ray_tile)
                    renderer.draw_dngn(ray_tile, x, y);

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
                    if (cell.blasphemy)
                        this.draw_dngn(dngn.BLASPHEMY, x, y);
                    if (cell.has_bfb_corpse)
                        this.draw_dngn(dngn.BLOOD_FOR_BLOOD, x, y);
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

                        // Monster difficulty. Ghosts get a special highlight.
                        if (fg.GHOST)
                        {
                            if (fg.TRIVIAL)
                                this.draw_dngn(dngn.THREAT_GHOST_TRIVIAL, x, y);
                            else if (fg.EASY)
                                this.draw_dngn(dngn.THREAT_GHOST_EASY, x, y);
                            else if (fg.TOUGH)
                                this.draw_dngn(dngn.THREAT_GHOST_TOUGH, x, y);
                            else if (fg.NASTY)
                                this.draw_dngn(dngn.THREAT_GHOST_NASTY, x, y);
                            else if (fg.UNUSUAL)
                                this.draw_dngn(dngn.THREAT_UNUSUAL, x, y);
                        }
                        else
                        {
                            if (fg.TRIVIAL)
                                this.draw_dngn(dngn.THREAT_TRIVIAL, x, y);
                            else if (fg.EASY)
                                this.draw_dngn(dngn.THREAT_EASY, x, y);
                            else if (fg.TOUGH)
                                this.draw_dngn(dngn.THREAT_TOUGH, x, y);
                            else if (fg.NASTY)
                                this.draw_dngn(dngn.THREAT_NASTY, x, y);
                            else if (fg.UNUSUAL)
                                this.draw_dngn(dngn.THREAT_UNUSUAL, x, y);
                        }

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

        draw_foreground: function(x, y, map_cell, img_scale)
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
                            this.draw_main(base_idx, x, y, img_scale);

                        this.draw_main(fg_idx, x, y, img_scale);
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
                            this.draw_main(base_idx, x, y, img_scale);

                        this.draw_main(fg_idx, x, y, img_scale);
                    }
                    finally
                    {
                        this.ctx.restore();
                    }
                }
                else
                {
                    if (base_idx)
                        this.draw_main(base_idx, x, y, img_scale);

                    this.draw_main(fg_idx, x, y, img_scale);
                }
            }
            else if (options.get("tile_display_mode") == "hybrid")
            {
                this.render_glyph(x, y, map_cell, true, true);
                this.draw_ray(x, y, cell);
                img_scale = undefined; // TODO: make this work?
            }

            if (fg.NET)
                this.draw_icon(icons.TRAP_NET, x, y, undefined, undefined, img_scale);

            if (fg.WEB)
                this.draw_icon(icons.TRAP_WEB, x, y, undefined, undefined, img_scale);

            if (fg.S_UNDER)
                this.draw_icon(icons.SOMETHING_UNDER, x, y, undefined, undefined, img_scale);

            // Pet mark
            if (fg.PET)
                this.draw_icon(icons.FRIENDLY, x, y, undefined, undefined, img_scale);
            else if (fg.GD_NEUTRAL)
                this.draw_icon(icons.GOOD_NEUTRAL, x, y, undefined, undefined, img_scale);
            else if (fg.NEUTRAL)
                this.draw_icon(icons.NEUTRAL, x, y, undefined, undefined, img_scale);

            var status_shift = 0;
            if (fg.PARALYSED)
            {
                this.draw_icon(icons.PARALYSED, x, y, undefined, undefined, img_scale);
                status_shift += 12;
            }
            else if (fg.STAB)
            {
                this.draw_icon(icons.STAB_BRAND, x, y, undefined, undefined, img_scale);
                status_shift += 12;
            }
            else if (fg.MAY_STAB)
            {
                this.draw_icon(icons.MAY_STAB_BRAND, x, y, undefined, undefined, img_scale);
                status_shift += 7;
            }
            else if (fg.FLEEING)
            {
                this.draw_icon(icons.FLEEING, x, y, undefined, undefined, img_scale);
                status_shift += 3;
            }

            if (fg.POISON)
            {
                this.draw_icon(icons.POISON, x, y, -status_shift, 0, img_scale);
                status_shift += 5;
            }
            else if (fg.MORE_POISON)
            {
                this.draw_icon(icons.MORE_POISON, x, y, -status_shift, 0, img_scale);
                status_shift += 5;
            }
            else if (fg.MAX_POISON)
            {
                this.draw_icon(icons.MAX_POISON, x, y, -status_shift, 0, img_scale);
                status_shift += 5;
            }

            for (var i = 0; i < cell.icons.length; ++i)
            {
                status_shift += this.draw_icon_type(cell.icons[i], x, y, -status_shift, 0, img_scale);
            }

            if (bg.UNSEEN && (bg.value || fg.value))
                this.draw_icon(icons.MESH, x, y, undefined, undefined, img_scale);

            if (bg.OOR && (bg.value || fg.value))
                this.draw_icon(icons.OOR_MESH, x, y, undefined, undefined, img_scale);

            if (bg.MM_UNSEEN && (bg.value || fg.value))
                this.draw_icon(icons.MAGIC_MAP_MESH, x, y, undefined, undefined, img_scale);

            if (bg.RAMPAGE)
                this.draw_icon(icons.RAMPAGE, x, y, undefined, undefined, img_scale);

            // Don't let the "new stair" icon cover up any existing icons, but
            // draw it otherwise.
            if (bg.NEW_STAIR && status_shift == 0)
                this.draw_icon(icons.NEW_STAIR, x, y, undefined, undefined, img_scale);

            if (bg.NEW_TRANSPORTER && status_shift == 0)
                this.draw_icon(icons.NEW_TRANSPORTER, x, y, undefined, undefined, img_scale);

            if (bg.EXCL_CTR && bg.UNSEEN)
                this.draw_icon(icons.TRAVEL_EXCLUSION_CENTRE_FG, x, y, undefined, undefined, img_scale);
            else if (bg.TRAV_EXCL && bg.UNSEEN)
                this.draw_icon(icons.TRAVEL_EXCLUSION_FG, x, y, undefined, undefined, img_scale);

            // Tutorial cursor takes precedence over other cursors.
            if (bg.TUT_CURSOR)
            {
                this.draw_icon(icons.TUTORIAL_CURSOR, x, y, undefined, undefined, img_scale);
            }
            else if (bg.CURSOR1)
            {
                this.draw_icon(icons.CURSOR, x, y, undefined, undefined, img_scale);
            }
            else if (bg.CURSOR2)
            {
                this.draw_icon(icons.CURSOR2, x, y, undefined, undefined, img_scale);
            }
            else if (bg.CURSOR3)
            {
                this.draw_icon(icons.CURSOR3, x, y, undefined, undefined, img_scale);
            }

            if (cell.travel_trail & 0xF)
            {
                this.draw_icon(icons.TRAVEL_PATH_FROM +
                               (cell.travel_trail & 0xF) - 1, x, y, undefined, undefined, img_scale);
            }
            if (cell.travel_trail & 0xF0)
            {
                this.draw_icon(icons.TRAVEL_PATH_TO +
                        ((cell.travel_trail & 0xF0) >> 4) - 1, x, y, undefined, undefined, img_scale);
            }

            if (fg.MDAM_LIGHT)
                this.draw_icon(icons.MDAM_LIGHTLY_DAMAGED, x, y, undefined, undefined, img_scale);
            else if (fg.MDAM_MOD)
                this.draw_icon(icons.MDAM_MODERATELY_DAMAGED, x, y, undefined, undefined, img_scale);
            else if (fg.MDAM_HEAVY)
                this.draw_icon(icons.MDAM_HEAVILY_DAMAGED, x, y, undefined, undefined, img_scale);
            else if (fg.MDAM_SEV)
                this.draw_icon(icons.MDAM_SEVERELY_DAMAGED, x, y, undefined, undefined, img_scale);
            else if (fg.MDAM_ADEAD)
                this.draw_icon(icons.MDAM_ALMOST_DEAD, x, y, undefined, undefined, img_scale);

            if (options.get("tile_show_demon_tier") === true)
            {
                if (fg.DEMON_1)
                    this.draw_icon(icons.DEMON_NUM1, x, y, undefined, undefined, img_scale);
                else if (fg.DEMON_2)
                    this.draw_icon(icons.DEMON_NUM2, x, y, undefined, undefined, img_scale);
                else if (fg.DEMON_3)
                    this.draw_icon(icons.DEMON_NUM3, x, y, undefined, undefined, img_scale);
                else if (fg.DEMON_4)
                    this.draw_icon(icons.DEMON_NUM4, x, y, undefined, undefined, img_scale);
                else if (fg.DEMON_5)
                    this.draw_icon(icons.DEMON_NUM5, x, y, undefined, undefined, img_scale);
            }
        },

        draw_icon_type: function(idx, x, y, ofsx, ofsy, img_scale)
        {
            switch (idx)
            {
                //These icons are in the lower right, so status_shift doesn't need changing.
                case icons.BERSERK:
                case icons.IDEALISED:
                case icons.TOUCH_OF_BEOGH:
                case icons.SHADOWLESS:
                // Anim. weap. and summoned might overlap, but that's okay
                case icons.SUMMONED:
                case icons.PERM_SUMMON:
                case icons.ANIMATED_WEAPON:
                case icons.VENGEANCE_TARGET:
                    this.draw_icon(idx, x, y, undefined, undefined, img_scale);
                    return 0;
                case icons.DRAIN:
                case icons.MIGHT:
                case icons.SWIFT:
                case icons.DAZED:
                case icons.HASTED:
                case icons.SLOWED:
                case icons.CORRODED:
                case icons.INFESTED:
                case icons.WEAKENED:
                case icons.PETRIFIED:
                case icons.PETRIFYING:
                case icons.BOUND_SOUL:
                case icons.POSSESSABLE:
                case icons.PARTIALLY_CHARGED:
                case icons.FULLY_CHARGED:
                case icons.VITRIFIED:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 6;
                case icons.CONC_VENOM:
                case icons.FIRE_CHAMP:
                case icons.INNER_FLAME:
                case icons.PAIN_MIRROR:
                case icons.STICKY_FLAME:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 7;
                case icons.ANGUISH:
                case icons.FIRE_VULN:
                case icons.RESISTANCE:
                case icons.GHOSTLY:
                case icons.MALMUTATED:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 8;
                case icons.RECALL:
                case icons.TELEPORTING:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 9;
                case icons.BLIND:
                case icons.BRILLIANCE:
                case icons.SLOWLY_DYING:
                case icons.WATERLOGGED:
                case icons.STILL_WINDS:
                case icons.ANTIMAGIC:
                case icons.REPEL_MISSILES:
                case icons.INJURY_BOND:
                case icons.GLOW_LIGHT:
                case icons.GLOW_HEAVY:
                case icons.BULLSEYE:
                case icons.CURSE_OF_AGONY:
                case icons.REGENERATION:
                case icons.RETREAT:
                case icons.RIMEBLIGHT:
                case icons.UNDYING_ARMS:
                case icons.BIND:
                case icons.SIGN_OF_RUIN:
                case icons.WEAK_WILLED:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 10;
                case icons.CONSTRICTED:
                case icons.VILE_CLUTCH:
                case icons.PAIN_BOND:
                    this.draw_icon(idx, x, y, ofsx, ofsy, img_scale);
                    return 11;
            }
        },

        // Helper functions for drawing from specific textures
        draw_tile: function(idx, x, y, mod, ofsx, ofsy, y_max, centre, img_scale)
        {
            // assumption: x and y are already appropriately scaled for the
            // canvas. Now we just need to figure out where in the scaled
            // cell size the tile belongs.
            var info = mod.get_tile_info(idx);
            var img = get_img(mod.get_img(idx));
            if (!info)
            {
                throw new Error("Tile not found: " + idx);
            }

            // this somewhat convoluted approach is to avoid fp scaling
            // artifacts at scale 1.0
            var img_xscale = this.x_scale;
            var img_yscale = this.y_scale;
            if (img_scale != undefined)
            {
                img_xscale = img_xscale * img_scale;
                img_yscale = img_yscale * img_scale;
            }
            else
                img_scale = 1.0;

            centre = centre === undefined ? true : centre;
            var size_ox = !centre ? 0 : 32 / 2 - info.w / 2;
            var size_oy = !centre ? 0 : 32 - info.h;
            var pos_sy_adjust = (ofsy || 0) + info.oy + size_oy;
            var pos_ey_adjust = pos_sy_adjust + info.ey - info.sy;
            var sy = pos_sy_adjust;
            var ey = pos_ey_adjust;
            if (y_max && y_max < ey)
                ey = y_max;

            if (sy >= ey)
                return;

            var total_x_offset = ((ofsx || 0) + info.ox + size_ox);

            // Offsets can be negative, in which case we are drawing overlapped
            // with a cell either to the right or above. If so, store the
            // overlap in cell state so that it can later be checked to trigger
            // a redraw.
            // These store logical pixels, but that currently doesn't matter
            // because they are only used for a comparison vs 0
            // See dungeon_renderer.js, render_loc.
            if (total_x_offset < this.current_left_overlap)
                this.current_left_overlap = total_x_offset;

            if (sy < this.current_sy)
                this.current_sy = sy;

            // dimensions in the source (the tilesheet)
            const w = info.ex - info.sx;
            const h = ey - sy;
            const ratio = window.devicePixelRatio;
            // dimensions in the target cell. To get this right at the pixel
            // level, we need to calculate the height/width as if it is offset
            // relative to the cell origin. Because of differences in how x/y
            // are treated above, these may look like they're doing something
            // different, but they shouldn't be.
            const scaled_w = Math.floor((total_x_offset + w) * img_xscale * ratio)
                                - Math.floor(total_x_offset * img_xscale * ratio);
            const scaled_h = Math.floor(ey * img_yscale * ratio)
                                - Math.floor(sy * img_yscale * ratio);

            this.ctx.imageSmoothingEnabled = options.get("tile_filter_scaling");
            this.ctx.drawImage(img,
                           info.sx, info.sy + sy - pos_sy_adjust, w, h,
                           x + Math.floor(total_x_offset * img_xscale * ratio),
                           y + Math.floor(sy * img_yscale * ratio),
                           scaled_w,
                           scaled_h);
        },

        draw_dngn: function(idx, x, y, img_scale)
        {
            this.draw_tile(idx, x, y, dngn,
                undefined, undefined, undefined, undefined,
                img_scale);
        },

        draw_gui: function(idx, x, y, img_scale)
        {
            this.draw_tile(idx, x, y, gui,
                undefined, undefined, undefined, undefined,
                img_scale);
        },

        draw_main: function(idx, x, y, img_scale)
        {
            this.draw_tile(idx, x, y, main,
                undefined, undefined, undefined, undefined,
                img_scale);
        },

        draw_player: function(idx, x, y, ofsx, ofsy, y_max, img_scale)
        {
            this.draw_tile(idx, x, y, tileinfo_player, ofsx, ofsy, y_max,
                undefined,
                img_scale);
        },

        draw_icon: function(idx, x, y, ofsx, ofsy, img_scale)
        {
            this.draw_tile(idx, x, y, icons, ofsx, ofsy,
                undefined, undefined,
                img_scale);
        },

        draw_quantity: function(qty, x, y, font)
        {
            qty = Math.max(0, Math.min(999, qty));

            this.ctx.fillStyle = "white";
            this.ctx.font = font;

            this.ctx.shadowColor = "black";
            this.ctx.shadowBlur = 2;
            this.ctx.shadowOffsetX = 1;
            this.ctx.shadowOffsetY = 1;
            this.ctx.textAlign = "left";
            this.ctx.textBaseline = "top";
            this.ctx.fillText(qty, (x + 2), (y + 2));
        },

        draw_from_texture: function (idx, x, y, tex, ofsx, ofsy, y_max, centre, img_scale)
        {
            var mod = tileinfos(tex);
            this.draw_tile(idx, x, y, mod, ofsx, ofsy, y_max, centre, img_scale);
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
