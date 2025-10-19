import $ from "jquery";

import comm from "./comm";

const line_span = $("<span>");
line_span.css("white-space", "pre");

function get_container(id) {
  return $("#ui-stack").children().last().find(`.${id}`);
}

function get_text_area_line(name, line) {
  const area = get_container(name);
  const lines = area.children("span");
  for (let i = lines.length; i <= line; ++i) {
    area.append(line_span.clone());
    area.append("<br>");
  }
  return area.children("span").eq(line);
}

function set_text_area_line(name, line, content) {
  const span = get_text_area_line(name, line);
  span.html(content);
}

function handle_text_update(data) {
  const area = get_container(data.id);
  if (data.clear) {
    const lines = area.children("span");
    for (let i = 0; i < lines.length; ++i) {
      if (!(i in data.lines)) lines.eq(i).empty();
    }
  }
  if (area.hasClass("menu_crt_shrink")) {
    const klist = Object.keys(data.lines);
    while (klist.length > 0) {
      const i = klist.pop();
      if (data.lines[i] !== "") break;
      delete data.lines[i];
    }
  }
  for (const line in data.lines)
    set_text_area_line(data.id, line, data.lines[line]);
  area.trigger("text_update");
}

comm.register_handlers({
  txt: handle_text_update,
});
