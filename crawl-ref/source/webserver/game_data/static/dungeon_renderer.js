define(["jquery", "comm", "./cell_renderer", "./map_knowledge", "./options",
    "./tileinfo-dngn", "./util", "./view_data", "./enums", "./mouse_control"],
function ($, comm, cr, map_knowledge, options, dngn, util, view_data, enums,
    mouse_control) {
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
        if (base_bg >= dngn.DNGN_LAVA && base_bg < dngn.FLOOR_MAX)
            return options.get("tile_water_anim");
        else if ((base_bg >= dngn.DNGN_ENTER_ZOT_CLOSED && base_bg < dngn.DNGN_CACHE_OF_FRUIT)
                 || (base_bg >= dngn.DNGN_SILVER_STATUE && base_bg < dngn.ARCANE_CONDUIT)
                 || (base_bg >= dngn.ARCANE_CONDUIT && base_bg < dngn.STORM_CONDUIT)
                 || is_torch(base_bg))
        {
            return options.get("tile_misc_anim");
        }
        else
            return false;
    }

    function animate_cell(cell)
    {
        var base_bg = dngn.basetile(cell.bg.value);
        if (base_bg == dngn.DNGN_PORTAL_WIZARD_LAB
            || base_bg == dngn.DNGN_ALTAR_JIYVA
            || base_bg >= dngn.ARCANE_CONDUIT && base_bg < dngn.STORM_CONDUIT
            || is_torch(base_bg))
        {
            cell.bg.value = base_bg + (cell.bg.value - base_bg + 1) % dngn.tile_count(base_bg);
        }
        else if ((base_bg > dngn.DNGN_LAVA && base_bg < dngn.BLOOD) ||
                 (base_bg >= dngn.DNGN_SILVER_STATUE && base_bg < dngn.ARCANE_CONDUIT))
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
        this.ui_state = -1;
        this.last_sent_cursor = { x: 0, y: 0 };
    }

    DungeonViewRenderer.prototype = new cr.DungeonCellRenderer();

    $.extend(DungeonViewRenderer.prototype, {
        init: function (element)
        {
            var renderer = this;
            $(element)
                .off("update_cells mousemove mouseleave mousedown")
                .on("update_cells", function (ev, cells) {
                    $.each(cells, function (i, loc) {
                        renderer.render_loc(loc.x, loc.y);
                    });
                })
                .on("mousemove mouseleave mousedown", function (ev) {
                    renderer.handle_mouse(ev);
                })

            cr.DungeonCellRenderer.prototype.init.call(this, element);
        },

        handle_mouse: function (ev)
        {
            if (!options.get("tile_web_mouse_control"))
                return;
            if (ev.type === "mouseleave")
            {
                if (this.tooltip_timeout)
                {
                    clearTimeout(this.tooltip_timeout);
                    this.tooltip_timeout = null;
                }

                view_data.remove_cursor(enums.CURSOR_MOUSE);
            }
            else
            {
                // convert logical mouse coordinates to device coordinates
                let scaled = this.scaled_size(ev.clientX, ev.clientY);
                let cell_size = this.scaled_size();
                var loc = {
                    x: Math.floor(scaled.width / cell_size.width) + this.view.x,
                    y: Math.floor(scaled.height / cell_size.height) + this.view.y
                };

                view_data.place_cursor(enums.CURSOR_MOUSE, loc);

                if (game.can_target())
                {
                    // XX refactor into mouse_control.js?
                    if (loc.x != this.last_sent_cursor.x
                        || loc.y != this.last_sent_cursor.y)
                    {
                        this.last_sent_cursor = {x: loc.x, y: loc.y};
                        comm.send_message("target_cursor",
                                                    this.last_sent_cursor);
                    }
                }

                if (ev.type === "mousemove")
                {

                    if (this.tooltip_timeout)
                        clearTimeout(this.tooltip_timeout);

                    var element = this.element;
                    this.tooltip_timeout = setTimeout(function () {
                        var new_ev = $.extend({}, ev, {
                            type: "cell_tooltip",
                            cell: loc
                        });
                        $(element).trigger(new_ev);
                    }, 500);
                }
                else if (ev.type === "mousedown")
                {
                    var new_ev = $.extend({}, ev, {
                        type: "cell_click",
                        cell: loc
                    });
                    $(this.element).trigger(new_ev);
                }
            }
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
            util.init_canvas(this.element,
                             c * this.cell_width,
                             r * this.cell_height);
            this.init(this.element);
            this.clear(); // clear the canvas and override default alpha fill
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

        // convert grid coordinates/dimensions to scaled canvas coordinates
        // by default, use map grid coordinates; set screen_grid to instead
        // use the visible grid (with 0,0 in the upper left of the screen)
        canvas_coords: function(cx, cy, cw, ch, screen_grid)
        {
            let scaled_cell = this.scaled_size();
            cw = cw || 1;
            ch = ch || 1;
            cx = cx || 0;
            cy = cy || 0;
            if (!screen_grid)
            {
                cx -= this.view.x;
                cy -= this.view.y;
            }
            return {
                x: cx * scaled_cell.width,
                y: cy * scaled_cell.height,
                width: cw * scaled_cell.width,
                height: ch * scaled_cell.height
            };
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

            const cw = this.cols - Math.abs(x);
            const ch = this.rows - Math.abs(y);

            let source = this.canvas_coords(sx, sy, cw, ch, true);

            if (source.width > 0 && source.height > 0)
            {
                // copy from source to dest
                // written a bit over-generally: don't really need to
                // recalculate destination size, unless this function is
                // ever used for rescaling
                let dest = this.canvas_coords(dx, dy, cw, ch, true);
                this.ctx.drawImage(this.element,
                    source.x, source.y, source.width, source.height,
                    dest.x, dest.y, dest.width, dest.height);
            }

            // Render cells that came into view
            for (var cy = 0; cy < dy; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_grid_cell(cx + this.view.x, cy + this.view.y);

            for (var cy = dy; cy < this.rows - sy; cy++)
            {
                for (var cx = 0; cx < dx; cx++)
                    this.render_grid_cell(cx + this.view.x, cy + this.view.y);
                for (var cx = this.cols - sx; cx < this.cols; cx++)
                    this.render_grid_cell(cx + this.view.x, cy + this.view.y);
            }

            for (var cy = this.rows - sy; cy < this.rows; cy++)
                for (var cx = 0; cx < this.cols; cx++)
                    this.render_grid_cell(cx + this.view.x, cy + this.view.y);
        },

        fit_to: function(width, height, min_diameter)
        {
            var scale;
            if (this.ui_state == enums.ui.VIEW_MAP)
                scale = options.get("tile_map_scale");
            else
                scale = options.get("tile_viewport_scale");
            var tile_size = Math.floor(options.get("tile_cell_pixels")
                                * scale / 100);
            var cell_size = {
                w: Math.floor(tile_size),
                h: Math.floor(tile_size)
            };

            if (options.get("tile_display_mode") == "glyphs")
            {
                // TODO: this should rescale to ensure los, similar to tiles
                this.glyph_mode_update_font_metrics();
                this.set_cell_size(this.glyph_mode_font_width,
                                    this.glyph_mode_line_height);
            }
            else if ((min_diameter * cell_size.w > width)
                || (min_diameter * cell_size.h > height))
            {
                // TODO: this produces bad results in hybrid mode, because the
                // font size isn't appropriately updated
                // scale down if necessary, so that los is in view
                var rescale = Math.min(width / (min_diameter * cell_size.w),
                                     height / (min_diameter * cell_size.h));
                this.set_cell_size(Math.floor(cell_size.w * rescale),
                                   Math.floor(cell_size.h * rescale));
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

        // render a single cell on the map grid
        render_grid_cell: function(cx, cy, map_cell, cell)
        {
            if (!this.in_view(cx, cy))
                return;
            map_cell = map_cell || map_knowledge.get(cx, cy);
            cell = cell || map_cell.t;
            let scaled = this.canvas_coords(cx, cy);
            this.render_cell(cx, cy, scaled.x, scaled.y, map_cell, cell);
        },

        // render a cell on the map grid, recursing as necessary based on
        // overlaps with adjacent cells below/right
        render_loc: function(cx, cy, map_cell)
        {
            this.render_grid_cell(cx, cy, map_cell);

            // Redraw the cell below if it overlapped
            if (this.in_view(cx, cy + 1))
            {
                let cell_below = map_knowledge.get(cx, cy + 1);
                if (cell_below.t && cell_below.t.sy && (cell_below.t.sy < 0))
                    this.render_loc(cx, cy + 1, cell_below);
            }
            // Redraw the cell to the right if it overlapped
            if (this.in_view(cx + 1, cy))
            {
                let cell_right = map_knowledge.get(cx + 1, cy);
                if (cell_right.t && cell_right.t.left_overlap
                    && (cell_right.t.left_overlap < 0))
                {
                    this.render_loc(cx + 1, cy, cell_right);
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
                if (map_knowledge.visible(map_cell) && cell_is_animated(cell))
                {
                    animate_cell(cell);
                    this.render_grid_cell(cx, cy, map_cell, cell);
                }
            }
        },

        // This is mostly here so that it can inherit cell size
        new_renderer: function(tiles)
        {
            tiles = tiles || [];
            var renderer = new cr.DungeonCellRenderer();
            var canvas = $("<canvas>");
            renderer.set_cell_size(this.cell_width,
                                   this.cell_height);
            util.init_canvas(canvas[0], this.cell_width, this.cell_height);
            renderer.init(canvas[0]);
            renderer.draw_tiles(tiles);

            return renderer;
        },

        set_ui_state: function(s)
        {
            this.ui_state = s;
        },

        update_mouse_mode: function (m)
        {
            if (!game.can_target())
                this.last_sent_cursor = { x: 0, y: 0 };
        },
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
        if (options.get("tile_realtime_anim"))
        {
            anim_interval = setInterval(function () {
                renderer.animate();
            }, 1000 / 4);
        }
    }

    options.add_listener(update_animation_interval);

    $(document).off("game_init.dungeon_renderer");
    $(document).on("game_init.dungeon_renderer", function () {
        renderer.cols = 0;
        renderer.rows = 0;
        renderer.view = { x: 0, y: 0 };
        renderer.view_center = { x: 0, y: 0 };

        renderer.init($("#dungeon")[0]);
    });

    $(document).off("game_cleanup.dungeon_renderer")
        .on("game_cleanup.dungeon_renderer", function () {
            if (anim_interval)
            {
                clearTimeout(anim_interval);
                anim_interval = null;
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
