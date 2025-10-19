import $ from "jquery";
import { createFocusTrap } from "focus-trap";

import comm from "./comm";
import client from "./client";

import options from "./options";

// Touch event duration
let touchstart = 0;

function wrap_popup(elem, ephemeral) {
  const wrapper = $(".templates > .ui-popup").clone();
  wrapper.data("ephemeral", ephemeral);
  wrapper.find(".ui-popup-inner").append(elem.removeClass("hidden"));
  return wrapper;
}

function unwrap_popup(wrapper) {
  console.assert(
    wrapper.hasClass("ui-popup"),
    "trying to unwrap something that hasn't been wrapped",
  );
  return wrapper.find(".ui-popup-inner").children();
}

function maybe_prevent_server_from_handling_key(ev) {
  const $focused = $(document.activeElement);
  if (!$focused.is("[data-sync-id]")) return;
  if ($focused.is("input[type=text]")) ev.stopPropagation();
  // stop server handling checkbox/button space/enter key events
  const checkbox = $focused.is("input[type=checkbox]");
  const button = $focused.is("input[type=button], button");
  const enter = ev.which === 13;
  const space = ev.which === 32;
  if ((checkbox || button) && (enter || space)) ev.stopPropagation();
}

function popup_keydown_handler(ev) {
  const wrapper = $("#ui-stack").children().last();
  const focused =
    document.activeElement !== document.body ? document.activeElement : null;

  // bail if inside a (legacy) input dialog (from line_reader)
  if ($(focused).closest("#input_dialog").length === 1) return;
  // or if outside the ui stack (probably the chat dialog)
  if ($(focused).closest("#ui-stack").length !== 1) return;

  if (ev.key === "Escape" && focused) {
    document.activeElement.blur();
    ev.stopPropagation();
  }

  if (ev.key === "Tab") {
    if (focused) {
      ev.stopPropagation();
      return;
    }

    const focusable = wrapper[0].querySelectorAll(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])',
    );
    if (focusable.length === 0) return;
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    (ev.shiftKey ? last : first).focus();
    ev.preventDefault();
    ev.stopPropagation();
  }
  ui_hotkey_handler(ev);
  maybe_prevent_server_from_handling_key(ev);
}

function popup_keypress_handler(ev) {
  maybe_prevent_server_from_handling_key(ev);
}

function target_outside_game(ev) {
  // I think this is for filtering clicks to the chat window, which is
  // supposed to work even with a popup in place
  return (
    $(ev.target).closest("#game").length !== 1 ||
    // Also allow zooming and scrolling on mobile browsers
    ev.type === "touchstart"
  );
}

function popup_clickoutside_handler(ev) {
  // this simulates the focus-trap click outside to deactivate code,
  // since for crawl we really need to send a "close popup" message
  // to the server rather than do it in the client.
  // TODO this lets through clicks that are in the popup's margin?
  // using parent() doesn't help with this issue because the space is
  // still in the border of ui-popup-outer. Maybe this is ok...
  if (!target_outside_game(ev) && !top_popup()[0].contains(ev.target)) {
    comm.send_message("key", { keycode: 27 });
    if (ev.type === "touchend") {
      // head off a `click` event, hopefully
      ev.preventDefault();
      return;
    }
  }
  // otherwise, ignore -- focus-trap checkPointerDown should get it
  // next.
}

function popup_touchstart_handler(_ev) {
  touchstart = Date.now();
}

function popup_touchend_handler(ev) {
  if (Date.now() - 500 < touchstart) popup_clickoutside_handler(ev);
}

function context_disable(ev) {
  // never disable the context menu for text input
  if (!$(ev.target).is(":input")) ev.preventDefault();
}

function disable_contextmenu() {
  document.addEventListener("contextmenu", context_disable, true);
}

function undisable_contextmenu() {
  document.removeEventListener("contextmenu", context_disable, true);
}

function show_popup(id, centred, generation_id) {
  const $ui_stack = $("#ui-stack");
  const elem = $(id);
  const ephemeral = elem.parent().length === 0;
  elem.detach();

  console.assert(elem.length === 1, "no popup to show");
  const wrapper = wrap_popup(elem, ephemeral);
  wrapper.toggleClass("centred", centred === true);
  wrapper.attr("data-generation-id", generation_id);
  $ui_stack.append(wrapper);
  wrapper
    .stop(true, true)
    .fadeIn(100, () => {
      if (client.is_watching()) return;
      wrapper[0].focus_trap = createFocusTrap(elem[0], {
        escapeDeactivates: false, // explicitly handled
        clickOutsideDeactivates: false, // explicitly handled
        fallbackFocus: document.body,
        onActivate: () => {
          if ($("#ui-stack").children().length === 1) {
            document.addEventListener("keydown", popup_keydown_handler, true);
            document.addEventListener("keypress", popup_keypress_handler, true);
            if (options.get("tile_web_mouse_control")) {
              document.addEventListener(
                "mousedown",
                popup_clickoutside_handler,
                true,
              );
              document.addEventListener(
                "touchstart",
                popup_touchstart_handler,
                true,
              );
              document.addEventListener(
                "touchend",
                popup_touchend_handler,
                true,
              );
            }
            disable_contextmenu();
          }
        },
        onDeactivate: () => {
          if ($("#ui-stack").children().length === 1) {
            document.removeEventListener(
              "keydown",
              popup_keydown_handler,
              true,
            );
            document.removeEventListener(
              "keypress",
              popup_keypress_handler,
              true,
            );
            if (options.get("tile_web_mouse_control")) {
              document.removeEventListener(
                "mousedown",
                popup_clickoutside_handler,
                true,
              );
              document.removeEventListener(
                "touchstart",
                popup_touchstart_handler,
                true,
              );
              document.removeEventListener(
                "touchend",
                popup_touchend_handler,
                true,
              );
            }
            undisable_contextmenu();
          }
        },
        allowOutsideClick: target_outside_game,
      }).activate();
    })
    .css("display", "");
  if (client.is_watching())
    wrapper.find("input, button").attr("disabled", true);
  if (elem.find(".paneset").length > 0) ui_resize_handler();
}

function hide_popup(show_below) {
  const $ui_stack = $("#ui-stack");
  const wrapper = $ui_stack.children().last();
  console.assert(wrapper.length === 1, "no popup to hide");

  let elem = unwrap_popup(wrapper).blur();
  if (!wrapper.data("ephemeral"))
    elem.detach().addClass("hidden").appendTo("body");
  if (wrapper[0].focus_trap) wrapper[0].focus_trap.deactivate();
  wrapper.remove();

  if (show_below === false) return;

  // Now show revealed menu
  const wrapper2 = $ui_stack.children().last();
  if (wrapper2.length > 0) {
    elem = unwrap_popup(wrapper2);
    wrapper2
      .stop(true, true)
      .fadeIn(100, () => {
        elem.focus();
      })
      .css("display", "");
  }
}

function top_popup() {
  const $popup = $("#ui-stack").children().last();
  if ($popup.length === 0) return;
  return $popup.find(".ui-popup-inner").children().eq(0);
}

function hide_all_popups() {
  const $ui_stack = $("#ui-stack");
  while ($ui_stack.children().length > 0) hide_popup(false);
}

function ui_key_handler(ev) {
  if (client.is_watching()) return;
  const $popup = top_popup();
  if ($popup === undefined) return;

  const new_ev = $.extend({}, ev);
  new_ev.type = ev.type.replace(/^game_/, "");
  $popup.triggerHandler(new_ev);

  if (new_ev.isDefaultPrevented()) ev.preventDefault();
  if (new_ev.isImmediatePropagationStopped()) ev.stopImmediatePropagation();
}

function ui_resize_handler(_ev) {
  if ($.browser.webkit) {
    $("#ui-stack .paneset").each((_i, el) => {
      $(el).children(".pane").css("height", "");
      const height = `${$(el).outerHeight()}px`;
      $(el).children(".pane").css("height", height);
    });

    $("#ui-stack [data-simplebar]").each((_i, el) => {
      $(el).data("scroller").recalculateImmediate();
    });
  }
}

function utf8_from_key_value(key) {
  if (key.length === 1) return key.charCodeAt(0);
  else
    switch (key) {
      case "Tab":
        return 9;
      case "Enter":
        return 13;
      case "Escape":
        return 27;
      default:
        return 0;
    }
}

function key_value_from_utf8(num) {
  switch (num) {
    case 9:
      return "Tab";
    case 13:
      return "Enter";
    case 27:
      return "Escape";
    default:
      return String.fromCharCode(num);
  }
}

function ui_hotkey_handler(ev) {
  if (ev.type === "click") {
    const key_value = $(ev.currentTarget).attr("data-hotkey");
    const keycode = utf8_from_key_value(key_value);
    if (keycode) comm.send_message("key", { keycode: keycode });
  } else if (ev.type === "keydown") {
    const $elem = $(`#ui-stack [data-hotkey="${event.key}"]`);
    if ($elem.length === 1) {
      $elem.click();
      ev.preventDefault();
      ev.stopPropagation();
    }
  }
}

function ui_input_handler(ev) {
  // this check is intended to fix a race condition when starting the
  // game: don't send any state info to the server until we have
  // received some. I'm not sure if it is really the right solution...
  if (!has_received_ui_state) return;
  const state_msg = {};
  sync_save_state(ev.target, state_msg);
  send_state_sync(ev.target, state_msg);
}

function ui_focus_handler(ev) {
  if (ev.type === "focusin") {
    const $elem = $(ev.target);
    if ($elem.closest(".ui-popup[data-generation-id]").length === 0) return;
    if (!$elem.is("[data-sync-id]")) {
      console.warn("tabbed to non-syncable element: ", $elem[0]);
      return;
    }
    send_state_sync($elem, { has_focus: true });
  } else if (ev.type === "focusout") {
    if (!ev.relatedTarget && top_popup())
      send_state_sync(top_popup(), { has_focus: true });
  }
}

const MO = new MutationObserver((mutations) => {
  mutations.forEach((record) => {
    const $chat = $(record.target);
    if ($chat.css("display") === "none") $chat[0].focus_trap.deactivate();
  });
});

function ui_chat_focus_handler(_ev) {
  if ($("#chat").hasClass("focus-trap")) return;

  const $chat = $("#chat");
  $chat[0].focus_trap = focus_trap($chat[0], {
    escapeDeactivates: true, // n.b. this apparently isn't doing
    // anything, over and above escape simply
    // hiding the chat div
    onActivate: () => {
      $chat.addClass("focus-trap");
      /* detect chat.js calling hide() via style attribute */
      MO.observe($chat[0], {
        attributes: true,
        attributeFilter: ["style"],
      });
    },
    onDeactivate: () => {
      $chat.removeClass("focus-trap");
    },
    returnFocusOnDeactivate: false,
    clickOutsideDeactivates: true,
  }).activate();
}

options.add_listener(() => {
  const size = options.get("tile_font_crt_size");
  $("#ui-stack").css("font-size", size === 0 ? "" : `${size}px`);

  let family = options.get("tile_font_crt_family");
  if (family !== "" && family !== "monospace") {
    family += ", monospace";
    $("#ui-stack").css("font-family", family);
  }

  $("#ui-stack").attr("data-display-mode", options.get("tile_display_mode"));
});

let has_received_ui_state = false;
$(document)
  .off("game_init.ui")
  .on("game_init.ui", () => {
    has_received_ui_state = false;
    $(document)
      .off("game_keydown.ui game_keypress.ui")
      .on("game_keydown.ui", ui_key_handler)
      .on("game_keypress.ui", ui_key_handler)
      .off("click.ui", "[data-hotkey]")
      .on("click.ui", "[data-hotkey]", ui_hotkey_handler)
      .off("input.ui focus.ui", "[data-sync-id]")
      .on("input.ui focus.ui", "[data-sync-id]", ui_input_handler)
      .off("focusin.ui focusout.ui")
      .on("focusin.ui focusout.ui", ui_focus_handler)
      .off("focusin.ui", "#chat_input")
      .on("focusin.ui", "#chat_input", ui_chat_focus_handler);
    $(window).off("resize.ui").on("resize.ui", ui_resize_handler);
  });

function sync_save_state(elem, state) {
  switch (elem.type) {
    case "text":
      state.text = elem.value;
      return;
    case "checkbox":
      state.checked = elem.checked;
      return;
  }
}

function sync_load_state(elem, state) {
  switch (elem.type) {
    case "text":
      elem.value = state.text;
      break;
    case "checkbox":
      elem.checked = state.checked;
      break;
    default:
      return;
  }
}

let receiving_ui_state = false;

function send_state_sync(elem, state) {
  if (receiving_ui_state) return;
  // TODO: remove the existence check once enough time has passed for
  // browser caches to expire
  if ((client.in_game && !client.in_game()) || client.is_watching()) return;
  const $target_popup = $(elem).closest("[data-generation-id]");
  if ($target_popup[0] !== top_popup().closest("[data-generation-id]")[0])
    return;
  state.generation_id = +$target_popup.attr("data-generation-id");
  state.widget_id = $(elem).attr("data-sync-id") || null;
  comm.send_message("ui_state_sync", state);
}

function sync_focus_state(elem) {
  if (client.is_watching())
    top_popup().find(".style-focused").removeClass("style-focused");
  if (elem) {
    elem.focus();
    if (client.is_watching()) $(elem).addClass("style-focused");
  } else document.activeElement.blur();
}

comm.register_handlers({
  "ui-state-sync": (msg) => {
    has_received_ui_state = true;
    if (msg.from_webtiles && !client.is_watching()) return;
    const popup = top_popup();
    if (!popup) return;
    const generation_id = +popup
      .closest("[data-generation-id]")
      .attr("data-generation-id");
    if (generation_id !== msg.generation_id) return;
    const elem =
      msg.widget_id !== ""
        ? popup.find(`[data-sync-id=${msg.widget_id}]`)[0]
        : null;
    try {
      receiving_ui_state = true;
      if (msg.has_focus) sync_focus_state(elem);
      else if (elem) sync_load_state(elem, msg);
    } finally {
      receiving_ui_state = false;
    }
  },
});

export default {
  show_popup: show_popup,
  hide_popup: hide_popup,
  top_popup: top_popup,
  disable_contextmenu: disable_contextmenu,
  undisable_contextmenu: undisable_contextmenu,
  target_outside_game: target_outside_game,
  hide_all_popups: hide_all_popups,
  utf8_from_key_value: utf8_from_key_value,
  key_value_from_utf8: key_value_from_utf8,
  sync_focus_state: sync_focus_state,
};
