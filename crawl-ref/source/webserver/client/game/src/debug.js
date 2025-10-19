import c from "client";
import $ from "jquery";
import display from "./display";
import r from "./dungeon_renderer";
import mk from "./map_knowledge";
import mm from "./minimap";
import ml from "./monster_list";

const exports = {};

exports.client = c;
exports.renderer = r;
exports.$ = $;
exports.minimap = mm;
exports.monster_list = ml;
exports.map_knowledge = mk;
exports.display = display;

window.debug = exports;

// Debug helper
exports.mark_cell = (x, y, mark) => {
  mark = mark || "m";

  if (mk.get(x, y).t) mk.get(x, y).t.mark = mark;

  r.render_loc(x, y);
};
exports.unmark_cell = (x, y) => {
  const cell = mk.get(x, y);
  if (cell) {
    delete cell.t.mark;
  }

  r.render_loc(x, y);
};
exports.mark_all = () => {
  const view = r.view;
  for (let x = 0; x < r.cols; x++)
    for (let y = 0; y < r.rows; y++)
      mark_cell(view.x + x, view.y + y, `${view.x + x}/${view.y + y}`);
};
exports.unmark_all = () => {
  const view = r.view;
  for (let x = 0; x < r.cols; x++)
    for (let y = 0; y < r.rows; y++) unmark_cell(view.x + x, view.y + y);
};

exports.obj_to_str = (o) => $.toJSON(o);

export default exports;
