import $ from "jquery";

import map_knowledge from "./map_knowledge";
import cr from "./cell_renderer";
import dungeon_renderer from "./dungeon_renderer";
import options from "./options";
import util from "./util";

let monsters, $list, monster_groups, max_rows;

function init() {
  monsters = {};
  $list = $("#monster_list");
  monster_groups = [];
  max_rows = 5;
}
$(document).bind("game_init", init);

function update_loc(loc) {
  const map_cell = map_knowledge.get(loc.x, loc.y);
  const mon = map_cell.mon;
  if (mon && map_knowledge.visible(map_cell))
    monsters[[loc.x, loc.y]] = { mon: mon, loc: loc };
  else delete monsters[[loc.x, loc.y]];
}

function can_combine(monster1, monster2) {
  return monster_sort(monster1, monster2) === 0;
}

function is_excluded(monster) {
  return (
    monster.typedata.no_exp &&
    !(
      monster.name === "active ballistomycete" || monster.name.match(/tentacle$/)
    )
  );
}

function monster_sort(m1, m2) {
  // Compare monster_info::less_than
  if (m1.att < m2.att) return -1;
  else if (m1.att > m2.att) return 1;

  if (m1.typedata.avghp > m2.typedata.avghp) return -1;
  else if (m1.typedata.avghp < m2.typedata.avghp) return 1;

  if (m1.type < m2.type) return 1;
  else if (m1.type > m2.type) return -1;

  // don't sort two same-name monsters together
  const m1Named = Object.hasOwn(m1, "clientid");
  const m2Named = Object.hasOwn(m2, "clientid");
  if (m1Named || m2Named) {
    if (!m2Named) return -1;
    if (!m1Named) return 1;
    if (m1.clientid < m2.clientid) return -1;
    return 1;
  }

  if (m1.name < m2.name) return 1;
  else if (m1.name > m2.name) return -1;

  return 0;
}

function group_monsters() {
  const monster_list = [];

  for (const loc in monsters) {
    if (is_excluded(monsters[loc].mon)) continue;
    monster_list.push(monsters[loc]);
  }

  monster_list.sort((m1, m2) => monster_sort(m1.mon, m2.mon));

  const new_monster_groups = [];
  let last_monster_group;
  for (let i = 0; i < monster_list.length; ++i) {
    const entry = monster_list[i];
    if (
      last_monster_group &&
      can_combine(last_monster_group[0].mon, entry.mon)
    ) {
      last_monster_group.push(entry);
    } else {
      last_monster_group = [monster_list[i]];
      new_monster_groups.push(last_monster_group);
    }
  }

  return new_monster_groups;
}

// These need to be updated when the respective enums change
const attitude_classes = [
  "hostile",
  "neutral",
  "good_neutral", // was strict_neutral
  "good_neutral",
  "friendly",
];
const mthreat_classes = ["trivial", "easy", "tough", "nasty"];

function update() {
  const grouped_monsters = group_monsters();

  const rows = Math.min(grouped_monsters.length, max_rows);
  let i = 0;
  for (; i < rows; ++i) {
    const monsters = grouped_monsters[i];
    let group, canvas, renderer;

    if (i >= monster_groups.length) {
      // Create a new row
      const node = $(
        "<span class='group'>\
                                <canvas class='picture'></canvas>\
                                <span class='health'></span>\
                                <span class='name'></span>\
                              </span>"
      );
      $list.append(node);
      canvas = node.find("canvas")[0];
      renderer = new cr.DungeonCellRenderer();
      group = {
        node: node,
        canvas: canvas,
        renderer: renderer,
        health_span: node.find(".health"),
        name_span: node.find(".name"),
        monsters: monsters,
      };
      monster_groups.push(group);
    } else {
      // Reuse the existing row
      group = monster_groups[i];
      canvas = group.canvas;
      renderer = group.renderer;
      group.monsters = monsters;
    }

    renderer.set_cell_size(
      dungeon_renderer.cell_width,
      dungeon_renderer.cell_height
    );
    if (options.get("tile_display_mode") !== "tiles") {
      for (const key in dungeon_renderer) {
        // dungeon_renderer.ui_state is also required so the glyph
        // size returned by the renderer.glyph_mode_font_name()
        // correctly reflects the 'tile_map_scale'
        if (key.match(/^glyph_mode/) || key === "ui_state")
          renderer[key] = dungeon_renderer[key];
      }
    }
    const w = renderer.cell_width;
    const displayed_monsters = Math.min(monsters.length, 6);
    const needed_width = w * displayed_monsters;
    if (
      canvas.style.width !== needed_width ||
      canvas.style.height !== dungeon_renderer.cell_height
    ) {
      util.init_canvas(canvas, needed_width, dungeon_renderer.cell_height);
      renderer.init(canvas);
    }

    for (let j = 0; j < displayed_monsters; ++j) {
      renderer.render_cell(monsters[j].loc.x, monsters[j].loc.y, j * w, 0);
    }

    if (monsters.length === 1) {
      group.name_span.text(monsters[0].mon.name);
      if (options.get("tile_display_mode") === "glyphs") {
        const map_cell = map_knowledge.get(monsters[0].loc.x, monsters[0].loc.y);
        const fg = map_cell.t.fg;
        let mdam = "uninjured";
        if (fg.MDAM_LIGHT) mdam = "lightly_damaged";
        else if (fg.MDAM_MOD) mdam = "moderately_damaged";
        else if (fg.MDAM_HEAVY) mdam = "heavily_damaged";
        else if (fg.MDAM_SEV) mdam = "severely_damaged";
        else if (fg.MDAM_ADEAD) mdam = "almost_dead";
        group.health_span
          .removeClass()
          .addClass(`${mdam} health`)
          .width(w)
          .height(renderer.cell_height)
          .show();
      } else group.health_span.hide();
    } else {
      group.name_span.text(`${monsters.length} ${monsters[0].mon.plural}`);
      group.health_span.hide();
    }

    $.each(attitude_classes, (_i, cls) => {
      group.name_span.removeClass(cls);
    });
    $.each(mthreat_classes, (_i, cls) => {
      group.name_span.removeClass(cls);
    });
    group.name_span.addClass(attitude_classes[monsters[0].mon.att]);
    group.name_span.addClass(mthreat_classes[monsters[0].mon.threat]);

    const ellipse = group.node.find(".ellipse");
    if (i === rows - 1 && rows < grouped_monsters.length) {
      if (ellipse.size() === 0)
        group.node.append("<span class='ellipse'>...</span>");
    } else ellipse.remove();
  }

  for (; i < monster_groups.length; ++i) {
    monster_groups[i].node.remove();
  }
  monster_groups = monster_groups.slice(0, rows);
}

function set_max_rows(x) {
  max_rows = x;
}

function clear() {
  $list.empty();
  monster_groups = [];
  monsters = {};
}

options.add_listener(() => {
  if (options.get("tile_font_lbl_size") === 0)
    $("#monster_list").css("font-size", "");
  else {
    $("#monster_list").css(
      "font-size",
      `${options.get("tile_font_lbl_size")}px`
    );
  }

  let family = options.get("tile_font_lbl_family");
  if (family !== "" && family !== "monospace") {
    family += ", monospace";
    $("#monster_list").css("font-family", family);
  }
});

export default {
  update_loc: update_loc,
  update: update,
  set_max_rows: set_max_rows,
  clear: clear,
  get: () => monster_groups,
};
