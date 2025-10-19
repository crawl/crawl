import $ from "jquery";

import comm from "./comm";
import client from "./client";

import map_knowledge from "./map_knowledge";
import messages from "./messages";
import options from "./options";
import util from "./util";

let player = {},
  last_time;

const stat_boosters = {
  str: "vitalised",
  int: "vitalised",
  dex: "vitalised",
  hp: "divinely vigorous|berserking",
  mp: "divinely vigorous",
};

/**
 * Update the stats area bar of the given type.
 * @param name     The name of the bar.
 * @param propname The player property name, if it is different from the
 *                 name of the bar defined in game_data/template/game.html
 */
function update_bar(name, propname) {
  if (!propname) propname = name;

  let value = player[propname],
    max = player[`${propname}_max`];
  let old_value;
  if (`old_${propname}` in player) old_value = player[`old_${propname}`];
  else old_value = value;
  if (value < 0) value = 0;
  if (old_value > max) old_value = max;
  player[`old_${propname}`] = value;
  const increase = old_value < value;
  // XX should both of these be floor?
  let full_bar = Math.round((10000 * (increase ? old_value : value)) / max);
  let change_bar = Math.floor((10000 * Math.abs(old_value - value)) / max);
  // Use poison_survival to display our remaining hp after poison expires.
  if (name === "hp") {
    $("#stats_hp_bar_poison").css("width", 0);
    const poison_survival = player.poison_survival;
    let poison_bar = 0;
    if (poison_survival < value) {
      poison_bar = Math.round((10000 * (value - poison_survival)) / max);
      full_bar = Math.round((10000 * poison_survival) / max);
      $("#stats_hp_bar_poison").css("width", `${poison_bar / 100}%`);
    }
    if (full_bar + poison_bar + change_bar > 10000)
      change_bar = 10000 - poison_bar - full_bar;
  } else if (full_bar + change_bar > 10000) {
    change_bar = 10000 - full_bar;
  }

  $(`#stats_${name}_bar_full`).css("width", `${full_bar / 100}%`);
  $(`#stats_${name}_bar_${increase ? "increase" : "decrease"}`).css(
    "width",
    `${change_bar / 100}%`
  );
  $(`#stats_${name}_bar_${increase ? "decrease" : "increase"}`).css("width", 0);
}

function update_bar_noise() {
  player.noise_max = 1000;
  let level = player.adjusted_noise,
    max = player.noise_max;
  let old_value;
  if ("old_noise" in player) old_value = player.old_noise;
  else old_value = level;
  if (level < 0) level = 0;
  if (old_value > max) old_value = max;

  // The adjusted_noise value has already been rescaled in
  // player.cc:get_adjusted_noise. It ranges from 0 to 1000.
  let adjusted_level = 0,
    noise_cat = "";

  // colors are set in `style.css` via the selector `data-level`.
  if (level <= 333) noise_cat = "quiet";
  else if (level <= 666) noise_cat = "loud";
  else if (level < 1000) noise_cat = "veryloud";
  else noise_cat = "superloud";

  const silenced = player.has_status("silence");
  if (silenced) {
    level = 0;
    old_value = 0;
    noise_cat = "blank";
    // I couldn't get this to work just directly putting the text in #stats_noise_bar,
    // because clearing the text later will adjust the span width.
    $("#stats_noise_status").text("Silenced");
  } else {
    $("#stats_noise_status").text("");
  }
  $("#stats_noise").attr("data-level", noise_cat);

  player.old_noise = level;

  const full_bar = Math.round((10000 * level) / max);
  let change_bar = Math.round((10000 * Math.abs(old_value - level)) / max);
  if (full_bar + change_bar > 10000) {
    change_bar = 10000 - full_bar;
  }

  if (player.wizard && !silenced) {
    // the exact value is too hard to interpret to show outside of
    // wizmode, because noise propagation is very complicated.
    $("#stats_noise").attr("data-shownum", true);
    $("#stats_noise_num").text(player.noise);
  } else {
    $("#stats_noise").attr("data-shownum", null);
    $("#stats_noise_num").text("");
  }

  $("#stats_noise_bar_full").css("width", `${full_bar / 100}%`);
  if (adjusted_level < old_value)
    $("#stats_noise_bar_decrease").css("width", `${change_bar / 100}%`);
  else $("#stats_noise_bar_decrease").css("width", 0);
}

function repeat_string(s, n) {
  return Array(n + 1).join(s);
}

function index_to_letter(index) {
  if (index === -1) return "-";
  else if (index < 26) return String.fromCharCode("a".charCodeAt(0) + index);
  else return String.fromCharCode("A".charCodeAt(0) + index - 26);
}
player.index_to_letter = index_to_letter;

function inventory_item_desc(index, parens = false) {
  const item = player.inv[index];
  const elem = $("<span>");
  if (parens) elem.text(`(${item.name})`);
  else elem.text(item.name);
  if (item.col !== -1 && item.col != null) elem.addClass(`fg${item.col}`);
  return elem;
}
player.inventory_item_desc = inventory_item_desc;

function wielded_weapon(offhand = false) {
  let elem;
  const wielded = offhand ? player.offhand_index : player.weapon_index;
  if (wielded === -1) {
    elem = $("<span>");
    elem.text(player.unarmed_attack);
    elem.addClass(`fg${player.unarmed_attack_colour}`);
  } else elem = inventory_item_desc(wielded);

  if (player.has_status("corroded")) elem.addClass("corroded_weapon");

  return elem;
}

function quiver() {
  // any use for player.quiver_available any more?
  return util.formatted_string_to_html(player.quiver_desc);
}

player.has_status_light = (status_light, col) => {
  for (let i = 0; i < player.status.length; ++i) {
    if (
      player.status[i].light?.match(status_light) &&
      (col == null || player.status[i].col === col)
    )
      return true;
  }
  return false;
};
player.has_status = (status_name, col) => {
  for (let i = 0; i < player.status.length; ++i) {
    if (
      player.status[i].text?.match(status_name) &&
      (col == null || player.status[i].col === col)
    )
      return true;
  }
  return false;
};
player.incapacitated = function incapacitated() {
  // FIXME: does this cover all ATTR_HELD cases?
  return (
    player.has_status("paralysed|petrified|sleeping") ||
    player.has_status("confused") ||
    player.has_status("held", 4)
  );
};

function update_defense(type) {
  const elem = $(`#stats_${type}`);
  elem.text(player[type]);
  elem.removeClass();

  if (type === "ac") {
    if (player.ac_mod > 0) elem.addClass("boosted_defense");
    else if (player.ac_mod < 0) elem.addClass("degenerated_defense");
  } else if (type === "ev") {
    if (player.ev_mod > 0) elem.addClass("boosted_defense");
    else if (player.ev_mod < 0) elem.addClass("degenerated_defense");
  } else if (type === "sh") {
    if (player.sh_mod > 0) elem.addClass("boosted_defense");
    else if (player.sh_mod < 0) elem.addClass("degenerated_defense");
  }
}

function stat_class(stat) {
  const val = player[stat];
  const max_val = player[`${stat}_max`];
  if (player.has_status(`lost ${stat}`)) return "zero_stat";

  const limits = options.get("stat_colour");
  for (const i in limits)
    if (val <= limits[i].value) return `colour_${limits[i].colour}`;

  if (player.has_status(stat_boosters[stat])) return "boosted_stat";

  if (val < max_val) return "degenerated_stat";

  return "";
}

function update_stat(stat) {
  const val = player[stat];
  const max_val = player[`${stat}_max`];
  const elem = $("<span>");
  elem.text(val);
  if (val < max_val) {
    elem.append(` (${max_val})`);
  }
  elem.addClass(stat_class(stat));
  $(`#stats_${stat}`).html(elem);
}

function update_doom() {
  if (player.doom === 0 && options.get("always_show_doom_contam") === false) {
    $("#stats_doom_ui").hide();
    return;
  } else $("#stats_doom_ui").show();

  const val = player.doom;
  const elem = $("<span>");
  elem.text(` ${val}%`);

  let colour = "fg7";
  if (player.doom === 0) colour = "fg8";
  if (player.doom >= 75) colour = "fg5";
  else if (player.doom >= 50) colour = "fg12";
  else if (player.doom >= 25) colour = "fg14";

  elem.addClass(colour);
  $("#stats_doom").html(elem);

  const tooltip = $("#stats_status_lights_tooltip");
  elem.on("mouseenter mousemove", (ev) => {
    tooltip.css({ top: `${ev.pageY}px` });
    tooltip.html(util.formatted_string_to_html(player.doom_desc));
    tooltip.show();
  });
  elem.on("mouseleave", (_ev) => tooltip.hide());
}

function update_contam() {
  if (player.contam === 0 && options.get("always_show_doom_contam") === false) {
    $("#stats_contam_ui").hide();
    return;
  } else $("#stats_contam_ui").show();

  const val = player.contam;
  const elem = $("<span>");
  elem.text(` ${val}%`);

  let colour = "fg8";
  if (val >= 200) colour = "fg4";
  else if (val >= 100) colour = "fg14";
  elem.addClass(colour);
  $("#stats_contam").html(elem);
}

function percentage_color(name) {
  let real = false;
  // There is only real_hp_max, real_mp_max doesn't exist
  if (
    player[`real_${name}_max`] &&
    player[`real_${name}_max`] !== player[`${name}_max`]
  )
    real = true;

  $(`#stats_${name}`).removeClass();
  $(`#stats_${name}_separator`).removeClass();
  $(`#stats_${name}_max`).removeClass();
  if (real) $(`#stats_real_${name}_max`).removeClass();

  if (player.has_status(stat_boosters[name])) {
    $(`#stats_${name}`).addClass("boosted_stat");
    $(`#stats_${name}_separator`).addClass("boosted_stat");
    $(`#stats_${name}_max`).addClass("boosted_stat");
    if (real) $(`#stats_real_${name}_max`).addClass("boosted_stat");
    return;
  }

  const val =
    (player[name] / player[`${(real ? "real_" : "") + name}_max`]) * 100;
  const limits = options.get(`${name}_colour`);
  let colour = null;
  for (const i in limits) if (val <= limits[i].value) colour = limits[i].colour;

  if (colour) $(`#stats_${name}`).addClass(`colour_${colour}`);
}

const simple_stats = ["hp", "hp_max", "mp", "mp_max", "xl", "progress"];
/**
 * Update the stats pane area based on the player's current properties.
 */
function update_stats_pane() {
  $("#stats_titleline").text(
    player.name + (player.title[0] === "," ? "" : " ") + player.title
  );
  $("#stats_wizmode").text(
    player.wizard ? "*WIZARD*" : player.explore ? "*EXPLORE*" : ""
  );

  // Setup species
  // TODO: Move to a proper initialisation task
  if ($("#stats").attr("data-species") !== player.species)
    $("#stats").attr("data-species", player.species);

  let species_god = player.species;
  if (player.god !== "") species_god += ` of ${player.god}`;
  if (player.god === "Xom") {
    if (player.piety_rank >= 0) {
      $("#stats_piety").text(
        repeat_string(".", player.piety_rank) +
          "*" +
          repeat_string(".", 5 - player.piety_rank)
      );
    } else $("#stats_piety").text("......"); // very special plaything
  } else if (
    (player.piety_rank > 0 || player.god !== "") &&
    player.god !== "Gozag"
  ) {
    $("#stats_piety").html(
      repeat_string("*", player.piety_rank) +
        repeat_string(".", 6 - player.piety_rank - player.ostracism_pips) +
        "<span class=fg5>" +
        repeat_string("X", player.ostracism_pips) +
        "</span>"
    );
  } else $("#stats_piety").text("");

  if (player.god === "Gozag") {
    $("#stats_gozag_gold_label").text(" Gold: ");
    $("#stats_gozag_gold_label").css("padding-left", "0.5em");
    $("#stats_gozag_gold").text(player.gold);
  } else {
    $("#stats_gozag_gold_label").text("");
    $("#stats_gozag_gold").text("");
    $("#stats_gozag_gold_label").css("padding-left", "0");
  }
  $("#stats_gozag_gold").toggleClass(
    "boosted_stat",
    !!player.has_status("gold aura")
  );

  $("#stats_species_god").text(species_god);
  $("#stats_piety").toggleClass("penance", !!player.penance);
  $("#stats_piety").toggleClass("monk", player.god === "");

  for (let i = 0; i < simple_stats.length; ++i)
    $(`#stats_${simple_stats[i]}`).text(player[simple_stats[i]]);

  $("#stats_hpline > .stats_caption").text(
    player.real_hp_max !== player.hp_max ? "HP:" : "Health:"
  );

  if (player.real_hp_max !== player.hp_max)
    $("#stats_real_hp_max").text(`(${player.real_hp_max})`);
  else $("#stats_real_hp_max").text("");

  if (
    player.species === "Deep Dwarf" &&
    player.dd_real_mp_max !== player.mp_max
  )
    $("#stats_dd_real_mp_max").text(`(${player.dd_real_mp_max})`);
  else $("#stats_dd_real_mp_max").text("");

  percentage_color("hp");
  percentage_color("mp");
  update_bar("hp");
  // is there a better place to do this?
  if (player.species === "Djinni") $("#stats_mpline").hide();
  else $("#stats_mpline").show();

  update_bar("mp");

  update_doom();
  update_contam();

  update_defense("ac");
  update_defense("ev");
  update_defense("sh");

  update_stat("str");
  update_stat("int");
  update_stat("dex");
  update_bar_noise();

  if (options.get("show_game_time") === true) {
    $("#stats_time_caption").text("Time:");
    $("#stats_time").text((player.time / 10.0).toFixed(1));
  } else {
    $("#stats_time_caption").text("Turn:");
    $("#stats_time").text(player.turn);
  }

  if (player.time_delta)
    $("#stats_time").append(` (${(player.time_delta / 10.0).toFixed(1)})`);

  let place_desc = player.place;
  if (player.depth) place_desc += `:${player.depth}`;
  $("#stats_place").text(place_desc);

  const tooltip = $("#stats_status_lights_tooltip");
  $("#stats_status_lights").html("");
  for (let i = 0; i < player.status.length; ++i) {
    const status_inf = player.status[i];
    if (!status_inf.light) continue;
    const status = $("<span>");
    status.addClass("status_light");
    status.addClass(`fg${status_inf.col}`);
    status.text(status_inf.light);
    status.on("mouseenter mousemove", (ev) => {
      tooltip.css({ top: `${ev.pageY}px` });
      tooltip.html(util.formatted_string_to_html(status_inf.desc));
      tooltip.show();
    });
    status.on("mouseleave", (_ev) => tooltip.hide());
    $("#stats_status_lights").append(status, " ");
  }

  $("#stats_weapon_letter").text(`${index_to_letter(player.weapon_index)})`);
  $("#stats_weapon").html(wielded_weapon());

  if (player.offhand_weapon)
    // Coglin dual wielding
    $("#stats_offhand_weapon_line").show();
  else $("#stats_offhand_weapon_line").hide();

  $("#stats_offhand_weapon_letter").text(
    `${index_to_letter(player.offhand_index)})`
  );
  $("#stats_offhand_weapon").html(wielded_weapon(true));

  $("#stats_quiver").html(quiver());
}

function handle_player_message(data) {
  for (const i in data.inv) {
    player.inv[i] = player.inv[i] || {};
    $.extend(player.inv[i], data.inv[i]);
    player.inv[i].slot = Number(i); // XX why is i a string?
  }

  if (data.inv) $("#action-panel").triggerHandler("update");

  delete data.inv;
  delete data.msg;

  $.extend(player, data);

  // chick if a forced player update has given us an explicit last
  // time to show in the hud for spectators (only). Otherwise, this value
  // is calculated on the client-side. (XX this works somewhat differently
  // than the console/tiles view; reconcile?)
  if ("time_last_input" in data) {
    // currently this message should *only* happen for spectators, but
    // just to be sure..
    // we don't want to do anything on this message to players,
    // because the server value can get out of sync with what the js
    // client is showing.
    if (client.is_watching())
      player.time_delta = player.time - player.time_last_input;

    last_time = player.time;
  } else if ("time" in data) {
    if (last_time) player.time_delta = player.time - last_time;

    last_time = player.time;
    messages.new_command(true);
  }

  update_stats_pane();

  if ("hp" in data || "hp_max" in data || "mp" in data || "mp_max" in data) {
    map_knowledge.touch(player.pos);
    $("#dungeon").trigger("update_cells", [[player.pos]]);
  }
}

options.add_listener(() => {
  if (player.name !== "") update_stats_pane();

  if (options.get("tile_font_stat_size") === 0)
    $("#stats").css("font-size", "");
  else {
    $("#stats").css("font-size", `${options.get("tile_font_stat_size")}px`);
  }

  let family = options.get("tile_font_stat_family");
  if (family !== "" && family !== "monospace") {
    family += ", monospace";
    $("#stats").css("font-family", family);
  }
});

comm.register_handlers({
  player: handle_player_message,
});

$(document)
  .off("game_init.player")
  .on("game_init.player", () => {
    $.extend(player, {
      name: "",
      god: "",
      title: "",
      species: "",
      hp: 0,
      hp_max: 0,
      real_hp_max: 0,
      poison_survival: 0,
      mp: 0,
      mp_max: 0,
      dd_real_mp_max: 0,
      ac: 0,
      ev: 0,
      sh: 0,
      xl: 0,
      progress: 0,
      time: 0,
      time_delta: 0,
      gold: 0,
      str: 0,
      int: 0,
      dex: 0,
      str_max: 0,
      int_max: 0,
      dex_max: 0,
      piety_rank: 0,
      penance: false,
      status: [],
      inv: {},
      weapon_index: -1,
      offhand_index: -1,
      quiver_item: -1,
      unarmed_attack: "",
      pos: { x: 0, y: 0 },
      wizard: 0,
      explore: 0,
      depth: 0,
      place: "",
      contam: 0,
      noise: 0,
      adjusted_noise: 0,
    });
    delete player.old_hp;
    delete player.old_mp;
    last_time = null;
  });

export default player;
