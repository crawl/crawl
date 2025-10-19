import $ from "jquery";
import { createFocusTrap } from "focus-trap";

import comm from "./comm";
import client from "./client";

import cr from "./cell_renderer";
import enums from "./enums";
import options from "./options";
import player from "./player";
import icons from "../../../game_data/static/tileinfo-icons";
import gui from "../../../game_data/static/tileinfo-gui";
import main from "../../../game_data/static/tileinfo-main";
import util from "./util";
import ui from "./ui";

let filtered_inv;
let renderer, $canvas, $settings, $tooltip;
let borders_width;
let minimized;
let settings_visible;
let tooltip_timeout = null;
// Options
let panel_disabled;
let scale, orientation, font_family, font_size;
let font; // cached font name for the canvas: size (in px) + family
let draw_glyphs;
let selected = -1;
const NUM_RESERVED_BUTTONS = 2;

function send_options() {
  if (client.is_watching()) return;
  options.set("action_panel_orientation", orientation, false);
  options.set("action_panel_show", !minimized, false);
  options.set("action_panel_scale", scale, false);
  options.set("action_panel_font_size", font_size, false);
  // TODO: some cleaner approach to this than multiple msgs
  options.send("action_panel_orientation");
  options.send("action_panel_show");
  options.send("action_panel_scale");
  options.send("action_panel_font_size");
}

function hide_panel(send_opts = true) {
  $("#action-panel-settings").hide();
  $("#action-panel").addClass("hidden");
  // if the player configured the panel to not show any items,
  // don't even show the placeholder button, don't update settings, etc.
  if (!panel_disabled) {
    $("#action-panel-placeholder").removeClass("hidden").show();
    // order of these two matters
    minimized = true;
    hide_settings();
    if (send_opts) send_options();
  }
}

function show_panel(send_opts = true) {
  if (settings_visible) return;
  $("#action-panel-settings").hide(); // sanitize
  $("#action-panel").removeClass("hidden");
  $("#action-panel-placeholder").addClass("hidden");
  minimized = false;
  if (send_opts) send_options();
}

function show_settings(e) {
  if (selected > 0) return false;
  hide_tooltip();
  const o_button = $(`#action-orient-${orientation}`);
  // Initialize the form with the current values
  o_button.prop("checked", true);

  // TODO: should these just reset to 100/16, rather than this somewhat
  // complicate context-sensitive behavior?
  // use parseInt to cut out any decimals
  const scale_percent = parseInt(scale, 10);
  $("#scale-val").val(scale_percent);
  if (!$("#scale-val").data("default"))
    $("#scale-val").data("default", scale_percent);

  $("#font-size-val").val(font_size);
  if (!$("#font-size-val").data("default"))
    $("#font-size-val").data("default", font_size);

  // Show the context menu near the cursor
  $settings = $("#action-panel-settings");
  $settings.css({ top: `${e.pageY + 10}px`, left: `${e.pageX + 10}px` });
  $settings.show();
  settings_visible = true;

  // TODO: I have had to set the buttons with tabindex -1, as I cannot
  // for the life of me get focus-trap to intercept their key input
  // correctly for esc and tab/shift-tab. This is non-ideal for
  // accessibility reasons.
  $settings[0].focus_trap = createFocusTrap($settings[0], {
    escapeDeactivates: true,
    onActivate: () => {
      $settings.addClass("focus-trap");
    },
    onDeactivate: () => {
      $settings.removeClass("focus-trap");
      // ugly to do this as a timeout, but it is the best way I've
      // found to get the key handling sequence right. This ensures
      // that if a mousedown event triggers deactivate, it is handled
      // while the settings panel is still open.
      setTimeout(hide_settings_final, 50);
    },
    returnFocusOnDeactivate: false,
    clickOutsideDeactivates: true,
  }).activate();

  return false;
}

function hide_settings() {
  if (!settings_visible) return;
  $settings[0].focus_trap.deactivate();
}

// triggered via focus-trap onDeactivate
function hide_settings_final() {
  $("#action-panel-settings").hide();
  settings_visible = false;

  // TODO: I can't quite figure out why the conditional is necessary, but
  // maybe something about the timing. Without it, sync_focus_state
  // steals focus from the chat window.
  if (!$("#chat").hasClass("focus-trap")) ui.sync_focus_state();

  // somewhat hacky: if hide_settings is triggered by hide_panel,
  // don't send options twice. Assumes that this flag is set first...
  if (!minimized) send_options();
}

function hide_tooltip() {
  if (tooltip_timeout) clearTimeout(tooltip_timeout);
  $tooltip.hide();
}

function show_tooltip(x, y, slot) {
  if (slot >= filtered_inv.length) {
    hide_tooltip();
    return;
  }
  $tooltip.css({ top: `${y + 10}px`, left: `${x + 10}px` });
  if (slot === -2) {
    $tooltip.html(
      "<span>Left click: minimize</span><br />" +
        "<span>Right click: open settings</span>",
    );
  } else if (slot === -1 && game.get_input_mode() === enums.mouse_mode.COMMAND)
    $tooltip.html("<span>Left click: show main menu</span>");
  else {
    const item = filtered_inv[slot];
    $tooltip.empty().text(`${String.fromCharCode(item.letter)} - `);
    $tooltip.append(player.inventory_item_desc(item.slot));
    if (game.get_input_mode() === enums.mouse_mode.COMMAND) {
      if (item.action_verb)
        $tooltip.append(
          "<br /><span>Left click: " +
            item.action_verb.toLowerCase() +
            "</span>",
        );
      $tooltip.append("<br /><span>Right click: describe</span>");
    }
  }
  $tooltip.show();
}

// Initial setup for the panel and its settings menu.
// Note that "game_init" happens before the client receives
// the options and inventory data from the server.
$(document).bind("game_init", () => {
  $canvas = $("#action-panel");
  $settings = $("#action-panel-settings");
  $tooltip = $("#action-panel-tooltip");
  if (client.is_watching()) {
    $canvas.addClass("hidden");
    return;
  }

  renderer = new cr.DungeonCellRenderer();
  borders_width = (parseInt($canvas.css("border-left-width"), 10) || 0) * 2;
  minimized = false;
  settings_visible = false;
  tooltip_timeout = null;
  filtered_inv = [];

  $canvas.on("update", update);

  $canvas.on("mousemove mouseleave mousedown mouseenter", (ev) => {
    handle_mouse(ev);
  });

  $canvas.contextmenu(() => false);

  // We don't need a context menu for the context menu
  $settings.contextmenu(() => false);

  // Clicking on the panel/Close button closes the settings menu
  $("#action-panel, #close-settings").click(() => {
    hide_settings();
  });

  // Triggering this function on keyup might be too aggressive,
  // but at least the player doesn't have to press Enter to confirm changes
  $("#action-panel-settings input[type=radio],input[type=number]").on(
    "change keyup",
    (e) => {
      if (e.which === 27) {
        hide_settings();
        e.preventDefault();
        return;
      }
      const input = e.target;
      if (input.type === "number" && !input.checkValidity()) return;
      options.set(input.name, input.value);
    },
  );

  $("#action-panel-settings button.reset").click(function () {
    const input = $(this).siblings("input");
    const default_value = input.data("default");
    input.val(default_value);
    options.set(input.prop("name"), default_value);
  });

  $("#minimize-panel").click(hide_panel);

  $("#action-panel-placeholder").click(() => {
    show_panel();
    update();
  });

  // To prevent the game from showing an empty panel before
  // any inventory data arrives, we hide it via inline CSS
  // and the "hidden" class. The next line deactivates
  // the inline rule, and the first call to update() will
  // remove "hidden" if the (filtered) inventory is not empty.
  $canvas.show();
});

function _horizontal() {
  return orientation === "horizontal";
}

function _update_font_props() {
  font = `${font_size || "16"}px ${font_family || "monospace"}`;
}

function handle_mouse(ev) {
  if (ev.type === "mouseleave") {
    selected = -1;
    hide_tooltip();
    update();
  } else {
    // focus-trap handles this case: the settings panel is about to
    // be closed and we should ignore the click
    if (ev.type === "mousedown" && settings_visible) return;

    // for finding the mouse position, we need to *not* use the device
    // pixel ratio to adjust the scale
    const cell_width = (renderer.cell_width * scale) / 100;
    const cell_height = (renderer.cell_height * scale) / 100;
    const _cell_length = _horizontal() ? cell_width : cell_height;
    const loc = {
      x: Math.round(ev.clientX / cell_width - 0.5),
      y: Math.round(ev.clientY / cell_height - 0.5),
    };

    if (ev.type === "mousemove" || ev.type === "mouseenter") {
      const oldselected = selected;
      selected = _horizontal() ? loc.x : loc.y;
      update();
      if (oldselected !== selected && !settings_visible) {
        hide_tooltip();
        tooltip_timeout = setTimeout(() => {
          show_tooltip(ev.pageX, ev.pageY, selected - NUM_RESERVED_BUTTONS);
        }, 500);
      }
    } else if (ev.type === "mousedown" && ev.which === 1) {
      if (selected === 0)
        // It should be available even in targeting mode
        hide_panel();
      else if (
        game.get_input_mode() === enums.mouse_mode.COMMAND &&
        selected === 1
      ) {
        comm.send_message("main_menu_action");
      } else if (
        game.get_input_mode() === enums.mouse_mode.COMMAND &&
        selected >= NUM_RESERVED_BUTTONS &&
        selected < filtered_inv.length + NUM_RESERVED_BUTTONS
      ) {
        comm.send_message("inv_item_action", {
          slot: filtered_inv[selected - NUM_RESERVED_BUTTONS].slot,
        });
      }
    } else if (ev.type === "mousedown" && ev.which === 3) {
      if (selected === 0)
        // right click on the x shows settings
        show_settings(ev);
      else if (
        game.get_input_mode() === enums.mouse_mode.COMMAND &&
        selected >= NUM_RESERVED_BUTTONS &&
        selected < filtered_inv.length + NUM_RESERVED_BUTTONS
      ) {
        comm.send_message("inv_item_describe", {
          slot: filtered_inv[selected - NUM_RESERVED_BUTTONS].slot,
        });
      }
    }
  }
}

function draw_action(
  texture,
  tiles,
  item,
  offset,
  scale,
  needs_cursor,
  text,
  useless,
) {
  if (item && draw_glyphs) {
    // XX just the glyph is not very informative. One idea might
    // be to tack on the subtype icon, but those are currently
    // baked into the item tile so this would be a lot of work.
    renderer.render_glyph(
      _horizontal() ? offset : 0,
      _horizontal() ? 0 : offset,
      item,
      true,
      true,
      scale,
    );
  } else {
    tiles = Array.isArray(tiles) ? tiles : [tiles];
    tiles.forEach((tile) => {
      renderer.draw_tile(
        tile,
        _horizontal() ? offset : 0,
        _horizontal() ? 0 : offset,
        texture,
        undefined,
        undefined,
        undefined,
        undefined,
        scale,
      );
    });
  }

  if (text) {
    // TODO: at some scalings, this don't dodge the green highlight
    // square very well
    renderer.draw_quantity(
      text,
      _horizontal() ? offset : 0,
      _horizontal() ? 0 : offset,
      font,
    );
  }

  if (needs_cursor) {
    renderer.draw_icon(
      icons.CURSOR3,
      _horizontal() ? offset : 0,
      _horizontal() ? 0 : offset,
      undefined,
      undefined,
      scale,
    );
  }

  if (useless) {
    renderer.draw_icon(
      icons.OOR_MESH,
      _horizontal() ? offset : 0,
      _horizontal() ? 0 : offset,
      undefined,
      undefined,
      scale,
    );
  }
}

function update() {
  if (client.is_watching()) return;

  // Have we received the inventory yet?
  // Note: an empty inventory will still have 52 empty slots.
  const inventory_initialized = Object.values(player.inv).length;
  if (!inventory_initialized) {
    $("#action-panel").addClass("hidden");
    return;
  }

  if (panel_disabled || minimized) {
    hide_panel(false);
    return;
  }

  // Filter
  filtered_inv = Object.values(player.inv).filter(
    (item) => item.quantity && item.action_panel_order >= 0,
  );

  // primary sort: determined by the `action_panel` option
  // secondary sort: determined by subtype
  filtered_inv.sort((a, b) => {
    if (a.action_panel_order === b.action_panel_order)
      return a.sub_type - b.sub_type;

    return a.action_panel_order - b.action_panel_order;
  });

  // Render
  const ratio = window.devicePixelRatio;

  // first we readjust the dimensions according to whether the panel
  // should be horizontal or vertical, and how much space is available.
  // These calculations are in logical pixels.
  const adjusted_scale = scale / 100;

  const cell_width = renderer.cell_width * adjusted_scale;
  const cell_height = renderer.cell_height * adjusted_scale;
  const cell_length = _horizontal() ? cell_width : cell_height;
  const required_length =
    cell_length * (filtered_inv.length + NUM_RESERVED_BUTTONS);
  let available_length = _horizontal()
    ? $("#dungeon").width()
    : $("#dungeon").height();
  available_length -= borders_width;
  const max_cells = Math.floor(available_length / cell_length);
  const panel_length = Math.min(required_length, available_length);

  util.init_canvas(
    $canvas[0],
    _horizontal() ? panel_length : cell_width,
    _horizontal() ? cell_height : panel_length,
  );
  renderer.init($canvas[0]);
  renderer.clear();

  // now draw the thing. From this point forward, use device pixels.
  const cell = renderer.scaled_size();
  const inc = (_horizontal() ? cell.width : cell.height) * adjusted_scale;

  // XX The "X" should definitely be a different/custom icon
  // TODO: select tile via something like c++ `tileidx_command`
  draw_action(gui, gui.PROMPT_NO, null, 0, adjusted_scale, selected === 0);
  draw_action(
    gui,
    gui.CMD_GAME_MENU,
    null,
    inc,
    adjusted_scale,
    selected === 1,
  );

  draw_glyphs = options.get("action_panel_glyphs");

  if (draw_glyphs) {
    // need to manually initialize for this type of renderer
    renderer.glyph_mode_font_size = options.get("glyph_mode_font_size");
    renderer.glyph_mode_font = options.get("glyph_mode_font");
    renderer.glyph_mode_update_font_metrics();
  }

  // Inventory items
  filtered_inv.slice(0, max_cells).forEach((item, idx) => {
    const offset = inc * (idx + NUM_RESERVED_BUTTONS);
    const qty_field_name = item.qty_field;
    let qty = "";
    if (Object.hasOwn(item, qty_field_name)) qty = item[qty_field_name];
    const cursor_required = selected === idx + NUM_RESERVED_BUTTONS;

    draw_action(
      main,
      item.tile,
      item,
      offset,
      adjusted_scale,
      cursor_required,
      qty,
      item.useless,
    );
  });

  if (available_length < required_length) {
    const ellipsis = icons.ELLIPSIS;
    let x_pos = 0,
      y_pos = 0;

    if (_horizontal())
      x_pos =
        available_length - icons.get_tile_info(ellipsis).w * adjusted_scale;
    else
      y_pos =
        available_length - icons.get_tile_info(ellipsis).h * adjusted_scale;

    renderer.draw_icon(
      ellipsis,
      x_pos * ratio,
      y_pos * ratio,
      -2,
      -2,
      adjusted_scale,
    );
  }
  $canvas.removeClass("hidden");
}

options.add_listener(() => {
  if (client.is_watching()) return;
  // synchronize visible state with new options. Because of messy timing
  // issues with the crawl binary, this will run at least twice on
  // startup.
  let update_required = false;

  const new_scale = options.get("action_panel_scale");
  if (scale !== new_scale) {
    scale = new_scale;
    update_required = true;
  }

  // is one of: horizontal, vertical
  const new_orientation = options.get("action_panel_orientation");
  if (orientation !== new_orientation) {
    orientation = new_orientation;
    update_required = true;
  }

  const new_min = !options.get("action_panel_show");
  if (new_min !== minimized) {
    minimized = new_min;
    update_required = true;
  }

  const new_font_family = options.get("action_panel_font_family");
  if (font_family !== new_font_family) {
    font_family = new_font_family;
    _update_font_props();
    update_required = true;
  }

  const new_font_size = options.get("action_panel_font_size");
  if (font_size !== new_font_size) {
    font_size = new_font_size;
    _update_font_props();
    update_required = true;
  }

  // The panel should be disabled completely only if the player
  // set the action_panel option to an empty string in the .rc
  panel_disabled = options.get("action_panel_disabled");

  if (update_required) {
    if (!minimized) show_panel(false);
    update();
  }
});
