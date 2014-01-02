define(["jquery", "./cell_renderer", "./map_knowledge", "./settings", "./options", "./tileinfo-dngn"],
function ($, cr, map_knowledge, settings, options, dngn) {
    "use strict";

    var global_anim_counter = 0;

    function is_torch(basetile)
    {
        return (basetile == dngn.WALL_BRICK_DARK_2_TORCH
                || basetile == dngn.WALL_BRICK_DARK_4_TORCH
                || basetile == dngn.WALL_BRICK_DARK_6_TORCH);
    }

    function cell_is_animated(cell)
    {
        if (cell == null || cell.bg == null) return false;
        var base_bg = dngn.basetile(cell.bg.value);
        return (base_bg == dngn.DNGN_PORTAL_WIZARD_LAB
                || is_torch(base_bg)
                || (base_bg >= dngn.DNGN_LAVA && base_bg < dngn.BLOOD));
    }

    function animate_cell(cell)
    {
        var base_bg = dngn.basetile(cell.bg.value);
        if (base_bg == dngn.DNGN_PORTAL_WIZARD_LAB
            || is_torch(base_bg))
        {
            cell.bg.value = base_bg + (cell.bg.value - base_bg + 1) % dngn.tile_count(base_bg);
        }
        else if (base_bg > dngn.DNGN_LAVA && base_bg < dngn.BLOOD)
        {
            cell.bg.value = base_bg + Math.floor(Math.random() * dngn.tile_count(base_bg))
        }
        else if (base_bg == dngn.DNGN_LAVA)
        {
            var tile = (cell.bg.value - base_bg) % 4;
            cell.bg.value = base_bg + (tile + 4 * global_anim_counter) % dngn.tile_count(base_bg);
        }
    }

    function DungeonViewRenderer()
    {
        cr.DungeonCellRenderer.call(this);

        this.cols = 0;
        this.rows = 0;

        this.view = { x: 0, y: 0 };
        this.view_center = { x: 0, y: 0 };
    }

    DungeonViewRenderer.prototype = new cr.DungeonCellRenderer();

    $.extend(DungeonViewRenderer.prototype, {
        init: function (element)
        {
            $(element).off("update_cells");
            $(element).on("update_cells", function (ev, cells) {
                $.each(cells, function (i, loc) {
                    renderer.render_loc(loc.x, loc.y);
                });
            });

            cr.DungeonCellRenderer.prototype.init.call(this, element);
        },

        set_size: function (c, r)
        {
            if ((this.cols == c) && (this.rows == r) &&
                (this.element.width == c * this.cell_width) &&
                (this.element.height == r * this.cell_height))
                return;

            this.cols = c;
            this.rows = r;
            this.view.x = this.view_center.x - Math.floor(c / 2);
            this.view.y = this.view_center.y - Math.floor(r / 2);
            this.element.width = c * this.cell_width;
            this.element.height = r * this.cell_height;
            this.init(this.element);
            // The canvas is completely transparent after resizing;
            // make it opaque to prevent display errors when shifting
            // around parts of it later.
            this.ctx.fillStyle = "black";
            this.ctx.fillRect(0, 0, this.element.width, this.element.height);
        },

        set_view_center: function(x, y)
        {
            this.view_center.x = x;
            this.view_center.y = y;
            var old_view = this.view;
            this.view = {
                x: x - Math.floor(this.cols / 2),
                y: y - Math.floor(this.rows / 2)
            };
            this.shift(this.view.x - old_view.x, this.view.y - old_view.y);
        },

        // Shifts the dungeon view by cx/cy cells.
        shift: function(x, y)
        {
            if ((x == 0) && (y == 0))
                return;

            if (x > this.cols) x = this.cols;
            if (x < -this.cols) x = -this.cols;
            if (y > this.rows) y = this.rows;
            if (y < -this.rows) y = -this.rows;

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

            var cw = this.cell_width;
            var ch = this.cell_height;

            var w = Math.floor((this.cols - Math.abs(x)) * cw);
            var h = Math.floor((this.rows - Math.abs(y)) * ch);

            if (w > 0 && h > 0)
            {
                var floor = Math.floor;
                this.ctx.drawImage(this.element,
                                   floor(sx * cw), floor(sy * ch), w, h,
                                   floor(dx * cw), floor(dy * ch), w, h);
            }

            // Render cells that came into view
            for (var cy = 0; cy < dy; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);

            for (var cy = dy; cy < this.rows - sy; cy++)
            {
                for (var cx = 0; cx < dx; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
                for (var cx = this.cols - sx; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
            }

            for (var cy = this.rows - sy; cy < this.rows; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_cell(cx + this.view.x, cy + this.view.y,
                                     cx * cw, cy * ch);
        },

        fit_to: function(width, height, min_diameter)
        {
            var cell_size = {
                w: options.get("tile_cell_pixels"),
                h: options.get("tile_cell_pixels")
            };

            if (this.display_mode == "glyphs")
            {
                this.ctx.font = this.glyph_mode_font_name();
                var metrics = this.ctx.measureText("@");
                this.set_cell_size(metrics.width + 2, this.glyph_mode_font_size + 2);
            }
            else if ((min_diameter * cell_size.w > width)
                || (min_diameter * cell_size.h > height))
            {
                var scale = Math.min(width / (min_diameter * cell_size.w),
                                     height / (min_diameter * cell_size.h));
                this.set_cell_size(Math.floor(cell_size.w * scale),
                                   Math.floor(cell_size.h * scale));
            }
            else
                this.set_cell_size(cell_size.w, cell_size.h);

            var view_width = Math.floor(width / this.cell_width);
            var view_height = Math.floor(height / this.cell_height);
            this.set_size(view_width, view_height);
        },

        in_view: function(cx, cy)
        {
            return (cx >= this.view.x) && (this.view.x + this.cols > cx) &&
                (cy >= this.view.y) && (this.view.y + this.rows > cy);
        },

        clear: function()
        {
            this.ctx.fillStyle = "black";
            this.ctx.fillRect(0, 0, this.element.width, this.element.height);
        },

        render_loc: function(cx, cy, map_cell)
        {
            map_cell = map_cell || map_knowledge.get(cx, cy);
            var cell = map_cell.t;

            if (this.in_view(cx, cy))
            {
                var x = (cx - this.view.x) * this.cell_width;
                var y = (cy - this.view.y) * this.cell_height;

                this.render_cell(cx, cy, x, y, map_cell, cell);

                // Need to redraw cell below if it overlapped
                // Redraw the cell below if it overlapped
                if (this.in_view(cx, cy + 1))
                {
                    var cell_below = map_knowledge.get(cx, cy + 1);
                    if (cell_below.t && cell_below.t.sy && (cell_below.t.sy < 0))
                        this.render_loc(cx, cy + 1);
                }
            }
        },

        animate: function ()
        {
            global_anim_counter++;
            if (global_anim_counter >= 65536) global_anim_counter = 0;

            for (var cy = this.view.y; cy < this.view.y + this.rows; ++cy)
                for (var cx = this.view.x; cx < this.view.x + this.cols; ++cx)
            {
                var map_cell = map_knowledge.get(cx, cy);
                var cell = map_cell.t;
                if (map_knowledge.visible(cx, cy) && cell_is_animated(cell))
                {
                    animate_cell(cell);
                    var x = (cx - this.view.x) * this.cell_width;
                    var y = (cy - this.view.y) * this.cell_height;
                    this.render_cell(cx, cy, x, y, map_cell, cell);
                }
            }
        },

        draw_overlay: function(idx, x, y)
        {
            if (this.in_view(x, y))
                this.draw_main(idx,
                               (x - this.view.x) * this.cell_width,
                               (y - this.view.y) * this.cell_height);
        },

        // This is mostly here so that it can inherit cell size
        new_renderer: function(tiles)
        {
            tiles = tiles || [];
            var renderer = new cr.DungeonCellRenderer();
            var canvas = $("<canvas>");
            renderer.set_cell_size(this.cell_width,
                                   this.cell_height);
            canvas[0].width = renderer.cell_width;
            canvas[0].height = renderer.cell_height;
            renderer.init(canvas[0]);
            renderer.draw_tiles(tiles);

            return renderer;
        }
    });

    var renderer = new DungeonViewRenderer();
    var anim_interval = null;

    function update_animation_interval()
    {
        if (anim_interval)
        {
            clearTimeout(anim_interval);
            anim_interval = null;
        }
        if (settings.get("animations"))
        {
            anim_interval = setInterval(function () {
                renderer.animate();
            }, 1000 / 4);
        }
    }

    $(document).off("game_init.dungeon_renderer");
    $(document).on("game_init.dungeon_renderer", function () {
        renderer.cols = 0;
        renderer.rows = 0;
        renderer.view = { x: 0, y: 0 };
        renderer.view_center = { x: 0, y: 0 };

        renderer.init($("#dungeon")[0]);

        update_animation_interval();
    });

    $(document).off("game_cleanup.dungeon_renderer")
        .on("game_cleanup.dungeon_renderer", function () {
            if (anim_interval)
            {
                clearTimeout(anim_interval);
                anim_interval = null;
            }
        });

    settings.set_defaults({ animations: false });
    $(document).off("settings_changed.dungeon_renderer")
        .on("settings_changed.dungeon_renderer", function (ev, map) {
            if ("animations" in map)
            {
                update_animation_interval();
            }
        });

    /* Hack to show animations (if real-time ones are not enabled)
       even in turns where nothing else happens */
    $(document).off("text_update.dungeon_renderer")
        .on("text_update.dungeon_renderer", function () {
            renderer.animate();
        });

    return renderer;
});
