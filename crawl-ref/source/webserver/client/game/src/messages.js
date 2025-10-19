import $ from "jquery";

import comm from "./comm";

import textinput from "./textinput";
import util from "./util";
import options from "./options";

let more = false;
let old_scroll_top;
const message_pane_height = 7; // default value, overridden by client

function hide() {
  old_scroll_top = $("#messages_container").scrollTop();
  $("#messages").hide();
}

function show() {
  $("#messages").show();
  $("#messages_container").scrollTop(old_scroll_top);
}

const _prefix_glyph_classes = "turn_marker command_marker";
function set_last_prefix_glyph(text, classes) {
  const last_msg_elem = $("#messages .game_message").last();
  const prefix_glyph = last_msg_elem.find(".prefix_glyph");
  prefix_glyph.text(text);
  prefix_glyph.addClass(classes);
}

function new_command(new_turn) {
  $("#more").hide();
  if (new_turn) set_last_prefix_glyph("_", "turn_marker");
  else set_last_prefix_glyph("_", "command_marker");
}

/**
    * Remove all message elements from the player messages window save for the
    * last height + 10.

    * This is necessary to prevent <div> elements from messages no longer in
    * view from pilling up over longer WebTiles session and thus slowing down
    * the browser.
    */
function remove_old_messages() {
  const all_messages = $("#messages .game_message");
  const keep = message_pane_height + 10;
  if (all_messages.length > keep) {
    const messages_to_remove = all_messages.slice(0, -keep);
    messages_to_remove.remove();
  }
}

function add_message(data) {
  remove_old_messages();
  const msg_elem = $("<div>");
  $("#messages").append(msg_elem);
  msg_elem.addClass("game_message");
  const prefix_glyph = $("<span></span>");
  prefix_glyph.addClass("prefix_glyph");
  prefix_glyph.html("&nbsp;");
  msg_elem.html(prefix_glyph);
  msg_elem.append(util.formatted_string_to_html(data.text));
  $("#messages_container").scrollTop($("#messages").height());
}

function rollback(count) {
  $("#messages .game_message").slice(-count).remove();
}

function handle_messages(msg) {
  if (msg.rollback) rollback(msg.rollback);
  if (msg.old_msgs) rollback(msg.old_msgs);
  if ("more" in msg) {
    more = msg.more;
    if ("more_text" in msg && msg.more_text.length > 0)
      $("#more").text(msg.more_text);
    else $("#more").text("--more--");
  }
  $("#more").toggle(more);
  if (msg.messages) {
    for (let i = 0; i < msg.messages.length; ++i) {
      add_message(msg.messages[i]);
    }
  }
  const cursor = $("#text_cursor");
  if (cursor.length > 0) cursor.appendTo($("#messages .game_message").last());
}

function messages_key_handler() {
  return !textinput.input_active();
}

function text_cursor(data) {
  if (data.enabled) {
    if ($("#text_cursor").length > 0) return;
    if (
      textinput.input_active() &&
      textinput.input_type() === "messages" &&
      textinput.find_input().length > 0
    ) {
      return;
    }
    const cursor = $("<span id='text_cursor'>_</span>");
    $("#messages .game_message").last().append(cursor);
  } else {
    $("#text_cursor").remove();
  }
}

options.add_listener(() => {
  if (options.get("tile_font_msg_size") === 0)
    $("#message_pane").css("font-size", "");
  else {
    $("#message_pane").css(
      "font-size",
      `${options.get("tile_font_msg_size")}px`
    );
  }

  let family = options.get("tile_font_msg_family");
  if (family !== "" && family !== "monospace") {
    family += ", monospace";
    $("#message_pane").css("font-family", family);
  }
});

comm.register_handlers({
  msgs: handle_messages,
  text_cursor: text_cursor,
});

$(document)
  .off("game_init.messages")
  .on("game_init.messages", () => {
    more = false;
    $(document)
      .off("game_keydown.messages game_keypress.messages")
      .on("game_keydown.messages", messages_key_handler)
      .on("game_keypress.messages", messages_key_handler);
  });

export default {
  hide: hide,
  show: show,
  new_command: new_command,
};
