import $ from "jquery";

import comm from "./comm";

import map_knowledge from "./map_knowledge";
import monster_list from "./monster_list";
import minimap from "./minimap";
import dungeon_renderer from "./dungeon_renderer";

function invalidate(minimap_too) {
  const b = map_knowledge.bounds();
  if (!b) return;

  const view = dungeon_renderer.view;

  const xs = minimap_too ? b.left : view.x;
  const ys = minimap_too ? b.top : view.y;
  const xe = minimap_too ? b.right : view.x + dungeon_renderer.cols - 1;
  const ye = minimap_too ? b.bottom : view.y + dungeon_renderer.rows - 1;
  for (let x = xs; x <= xe; x++)
    for (let y = ys; y <= ye; y++) {
      map_knowledge.touch(x, y);
    }
}

function display() {
  // Update the display.
  if (!map_knowledge.bounds()) return;

  const t1 = new Date();

  if (map_knowledge.reset_bounds_changed()) minimap.center();

  const dirty_locs = map_knowledge.dirty();
  for (let i = 0; i < dirty_locs.length; i++) {
    const loc = dirty_locs[i];
    const cell = map_knowledge.get(loc.x, loc.y);
    cell.dirty = false;
    monster_list.update_loc(loc);
    dungeon_renderer.render_loc(loc.x, loc.y, cell);
    minimap.update(loc.x, loc.y, cell);
  }
  map_knowledge.reset_dirty();

  dungeon_renderer.animate();

  monster_list.update();

  const render_time = Date.now() - t1;
  if (!window.render_times) window.render_times = [];
  if (window.render_times.length >= 20) window.render_times.shift();
  window.render_times.push(render_time);
}

function clear_map() {
  map_knowledge.clear();

  dungeon_renderer.clear();

  minimap.clear();

  monster_list.clear();
}

// Message handlers
function handle_map_message(data) {
  if (data.clear) clear_map();

  if (data.player_on_level != null)
    map_knowledge.set_player_on_level(data.player_on_level);

  if (data.vgrdc) minimap.do_view_center_update(data.vgrdc.x, data.vgrdc.y);

  if (data.cells) map_knowledge.merge(data.cells);

  // Mark cells overlapped by dirty cells as dirty
  $.each(map_knowledge.dirty().slice(), (_i, loc) => {
    const cell = map_knowledge.get(loc.x, loc.y);
    // high cell
    if (cell.t?.sy && cell.t.sy < 0) map_knowledge.touch(loc.x, loc.y - 1);
    // left overlap
    if (cell.t?.left_overlap && cell.t.left_overlap < 0) {
      map_knowledge.touch(loc.x - 1, loc.y);
      // If we overlap at both top *and* left, we may additionally
      // overlap diagonally.
      if (cell.t.sy && cell.t.sy < 0) map_knowledge.touch(loc.x - 1, loc.y - 1);
    }
  });

  display();
}

function _handle_vgrdc(_data) {}

comm.register_handlers({
  map: handle_map_message,
});

export default {
  invalidate: invalidate,
  display: display,
};
