---------------------------------------------------------------------------
-- autofight.lua:
-- One-key fighting.
--
-- To use this, please bind a key to the following commands:
-- ===hit_closest         (Tab by default)
-- ===hit_closest_nomove  (Shift-Tab by default)
-- ===toggle_autothrow    (not bound by default)
--
-- This uses the very incomplete client monster and view bindings, and
-- is currently very primitive. Improvements welcome!
---------------------------------------------------------------------------

local ATT_HOSTILE = 0
local ATT_NEUTRAL = 1

local LOS_RADIUS = 7

AUTOFIGHT_STOP = 50
AUTOFIGHT_HUNGER_STOP = 0
AUTOFIGHT_HUNGER_STOP_UNDEAD = false
AUTOFIGHT_CAUGHT = false
AUTOFIGHT_THROW = false
AUTOFIGHT_THROW_NOMOVE = true
AUTOFIGHT_FIRE_STOP = false
AUTOFIGHT_WAIT = false
AUTOFIGHT_PROMPT_RANGE = true
AUTOMAGIC_ACTIVE = false

local function delta_to_vi(dx, dy)
  local d2v = {
    [-1] = { [-1] = "CMD_MOVE_UP_LEFT",  [0] = "CMD_MOVE_LEFT",  [1] = "CMD_MOVE_DOWN_LEFT"},
    [0]  = { [-1] = "CMD_MOVE_UP",                               [1] = "CMD_MOVE_DOWN"},
    [1]  = { [-1] = "CMD_MOVE_UP_RIGHT", [0] = "CMD_MOVE_RIGHT", [1] = "CMD_MOVE_DOWN_RIGHT"},
  }
  return d2v[dx][dy]
end

local function target_delta_to_vi(dx, dy)
  local d2v = {
    [-1] = { [-1] = "CMD_TARGET_UP_LEFT",  [0] = "CMD_TARGET_LEFT",  [1] = "CMD_TARGET_DOWN_LEFT"},
    [0]  = { [-1] = "CMD_TARGET_UP",                                 [1] = "CMD_TARGET_DOWN"},
    [1]  = { [-1] = "CMD_TARGET_UP_RIGHT", [0] = "CMD_TARGET_RIGHT", [1] = "CMD_TARGET_DOWN_RIGHT"},
  }
  return d2v[dx][dy]
end

local function sign(a)
  return a > 0 and 1 or a < 0 and -1 or 0
end

local function abs(a)
  return a * sign(a)
end

local function adjacent(dx, dy)
  return abs(dx) <= 1 and abs(dy) <= 1
end

local function vector_move(a, dx, dy)
  for i = 1,abs(dx) do
    a[#a+1] = target_delta_to_vi(sign(dx), 0)
  end
  for i = 1,abs(dy) do
    a[#a+1] = target_delta_to_vi(0, sign(dy))
  end
end

local function have_reaching()
  local wp = items.equipped_at("weapon")
  return wp and wp.reach_range == 2 and not wp.is_melded
end

local function have_ranged()
  local wp = items.equipped_at("weapon")
  return wp and wp.is_ranged and not wp.is_melded
end

local function have_throwing(no_move)
  return (AUTOFIGHT_THROW or no_move and AUTOFIGHT_THROW_NOMOVE) and items.fired_item() ~= nil
end

local function is_safe_square(dx, dy)
    if view.feature_at(dx, dy) == "trap_web" then
        return false
    end
    return view.is_safe_square(dx, dy)
end

local function can_move_maybe(dx, dy)
  if view.feature_at(dx,dy) ~= "unseen" and view.is_safe_square(dx,dy) then
    local m = monster.get_monster_at(dx, dy)
    if not m or not m:is_firewood() then
      return true
    end
  end
  return false
end

local function can_move_now(dx, dy)
  local m = monster.get_monster_at(dx, dy)
  -- attitude > ATT_NEUTRAL should mean you can push past the monster
  return (is_safe_square(dx, dy) and (not m or m:attitude() > ATT_NEUTRAL))
end

local function choose_move_towards(ax, ay, bx, by, square_func)
  local move = nil
  local dx = bx - ax
  local dy = by - ay
  local function try_move(mx, my)
    if mx == 0 and my == 0 then
      return nil
    elseif abs(ax+mx) > LOS_RADIUS or abs(ay+my) > LOS_RADIUS then
      return nil
    elseif square_func(ax+mx, ay+my) then
      return {mx,my}
    else
      return nil
    end
  end
  if abs(dx) > abs(dy) then
    if abs(dy) == 1 then
      move = try_move(sign(dx), 0)
    end
    if move == nil then move = try_move(sign(dx), sign(dy)) end
    if move == nil then move = try_move(sign(dx), 0) end
    if move == nil and abs(dx) > abs(dy)+1 then
      move = try_move(sign(dx), 1)
    end
    if move == nil and abs(dx) > abs(dy)+1 then
      move = try_move(sign(dx), -1)
    end
    if move == nil then move = try_move(0, sign(dy)) end
  elseif abs(dx) == abs(dy) then
    move = try_move(sign(dx), sign(dy))
    if move == nil then move = try_move(sign(dx), 0) end
    if move == nil then move = try_move(0, sign(dy)) end
  else
    if abs(dx) == 1 then
      move = try_move(0, sign(dy))
    end
    if move == nil then move = try_move(sign(dx), sign(dy)) end
    if move == nil then move = try_move(0, sign(dy)) end
    if move == nil and abs(dy) > abs(dx)+1 then
      move = try_move(1, sign(dy))
    end
    if move == nil and abs(dy) > abs(dx)+1 then
      move = try_move(-1, sign(dy))
    end
    if move == nil then move = try_move(sign(dx), 0) end
  end
  return move
end

local function move_towards(dx, dy)
  local move = choose_move_towards(0, 0, dx, dy, can_move_now)
  if move == nil then
    crawl.mpr("Failed to move towards target.")
  else
    crawl.do_commands({delta_to_vi(move[1],move[2])})
  end
end

local function will_tab(ax, ay, bx, by)
  if abs(bx-ax) <= 1 and abs(by-ay) <= 1 or
     abs(bx-ax) <= 2 and abs(by-ay) <= 2 and have_reaching() then
    return true
  end
  local move = choose_move_towards(ax, ay, bx, by, can_move_maybe)
  if move == nil then
    return false
  end
  return will_tab(ax+move[1], ay+move[2], bx, by)
end

local function get_monster_info(dx,dy,no_move)
  m = monster.get_monster_at(dx,dy)
  name = m:name()
  if not m then
    return nil
  end
  info = {}
  info.distance = (abs(dx) > abs(dy)) and -abs(dx) or -abs(dy)
  if have_ranged() then
    info.attack_type = you.see_cell_no_trans(dx, dy) and 3 or 0
  elseif not have_reaching() then
    info.attack_type = (-info.distance < 2) and 2 or 0
  else
    if -info.distance > 2 then
      info.attack_type = 0
    elseif -info.distance < 2 then
      info.attack_type = 2
    else
      info.attack_type = view.can_reach(dx, dy) and 1 or 0
    end
  end
  if info.attack_type == 0 and have_throwing(no_move) and you.see_cell_no_trans(dx, dy) then
    -- Melee is better than throwing.
    info.attack_type = 3
  end
  if info.attack_type == 0 and not will_tab(0,0,dx,dy) then
    info.attack_type = -1
  end
  info.can_attack = (info.attack_type > 0) and 1 or info.attack_type
  info.safe = m:is_safe() and -1 or 0
  info.constricting_you = m:is_constricting_you() and 1 or 0
  -- Only prioritize good stabs: sleep and paralysis.
  info.very_stabbable = (m:stabbability() >= 1) and 1 or 0
  info.injury = m:damage_level()
  info.threat = m:threat()
  info.orc_priest_wizard = (name == "orc priest" or name == "orc wizard") and 1 or 0
  return info
end

local function compare_monster_info(m1, m2)
  flag_order = autofight_flag_order
  if flag_order == nil then
    flag_order = {"can_attack", "safe", "distance", "constricting_you", "very_stabbable", "injury", "threat", "orc_priest_wizard"}
  end
  for i,flag in ipairs(flag_order) do
    if m1[flag] > m2[flag] then
      return true
    elseif m1[flag] < m2[flag] then
      return false
    end
  end
  return false
end

local function is_candidate_for_attack(x,y)
  m = monster.get_monster_at(x, y)
  --if m then crawl.mpr("Checking: (" .. x .. "," .. y .. ") " .. m:name()) end
  if not m or m:attitude() ~= ATT_HOSTILE then
    return false
  end
  if m:name() == "butterfly"
      or m:name() == "orb of destruction" then
    return false
  end
  if m:is_firewood() then
  --crawl.mpr("... is firewood.")
    if string.find(m:name(), "ballistomycete") then
      return true
    end
    return false
  end
  return true
end

local function get_target(no_move)
  local x, y, bestx, besty, best_info, new_info
  bestx = 0
  besty = 0
  best_info = nil
  for x = -LOS_RADIUS,LOS_RADIUS do
    for y = -LOS_RADIUS,LOS_RADIUS do
      if is_candidate_for_attack(x, y) then
        new_info = get_monster_info(x, y, no_move)
        if (not best_info) or compare_monster_info(new_info, best_info) then
          bestx = x
          besty = y
          best_info = new_info
        end
      end
    end
  end
  return bestx, besty, best_info
end

local function attack_fire(x,y)
  local a = {"CMD_FIRE", "CMD_TARGET_FIND_YOU"}
  vector_move(a, x, y)
  a[#a+1] = "CMD_TARGET_SELECT"
  crawl.do_commands(a, true)
end

local function attack_fire_stop(x,y)
  local a = {"CMD_FIRE", "CMD_TARGET_FIND_YOU"}
  vector_move(a, x, y)
  a[#a+1] = "CMD_TARGET_SELECT_ENDPOINT"
  crawl.do_commands(a, true)
end

local function attack_reach(x,y)
  local a = {"CMD_EVOKE_WIELDED", "CMD_TARGET_FIND_YOU"}
  vector_move(a, x, y)
  a[#a+1] = "CMD_TARGET_SELECT_ENDPOINT"
  crawl.do_commands(a, true)
end

local function attack_melee(x,y)
  crawl.do_commands({delta_to_vi(x, y)})
end

local function set_stop_level(key, value, mode)
  AUTOFIGHT_STOP = tonumber(value)
end

local function set_hunger_stop_level(key, value, mode)
  AUTOFIGHT_HUNGER_STOP = tonumber(value)
end

local function set_hunger_stop_undead(key, value, mode)
  AUTOFIGHT_HUNGER_STOP_UNDEAD = string.lower(value) ~= "false"
end

local function set_af_caught(key, value, mode)
  AUTOFIGHT_CAUGHT = string.lower(value) ~= "false"
end

local function set_af_throw(key, value, mode)
  AUTOFIGHT_THROW = string.lower(value) ~= "false"
end

local function set_af_throw_nomove(key, value, mode)
  AUTOFIGHT_THROW_NOMOVE = string.lower(value) ~= "false"
end

local function set_af_fire_stop(key, value, mode)
  AUTOFIGHT_FIRE_STOP = string.lower(value) ~= "false"
end

local function set_af_wait(key, value, mode)
    AUTOFIGHT_WAIT = string.lower(value) ~= "false"
end

local function set_af_prompt_range(key, value, mode)
    AUTOFIGHT_PROMPT_RANGE = string.lower(value) ~= "false"
end

function set_automagic(key, value, mode)
  AUTOMAGIC_ACTIVE = string.lower(value) ~= "false"
end

function af_hp_is_low()
  local hp, mhp = you.hp()
  return (100*hp <= AUTOFIGHT_STOP*mhp)
end

function af_food_is_low()
  if you.race() == "Mummy" or you.transform() == "lich" then
      return false
  elseif (not AUTOFIGHT_HUNGER_STOP_UNDEAD)
         and (you.race() == "Vampire" or you.race() == "Ghoul") then
      return false
  else
      return (AUTOFIGHT_HUNGER_STOP ~= nil
              and you.hunger() <= AUTOFIGHT_HUNGER_STOP)
  end
end

function attack(allow_movement)
  local x, y, info = get_target(not allow_movement)
  local caught = you.caught()
  if af_hp_is_low() then
    crawl.mpr("You are too injured to fight recklessly!")
  elseif af_food_is_low() then
    crawl.mpr("You are too hungry to fight recklessly!")
  elseif you.confused() then
    crawl.mpr("You are too confused!")
  elseif caught then
    if AUTOFIGHT_CAUGHT then
      crawl.do_commands({delta_to_vi(1, 0)}) -- Direction doesn't matter.
    else
      crawl.mpr("You are " .. caught .. "!")
    end
  elseif info == nil then
    if AUTOFIGHT_WAIT and not allow_movement then
      crawl.do_commands({"CMD_WAIT"})
    else
      crawl.mpr("No target in view!")
    end
  elseif info.attack_type == 3 then
    if AUTOFIGHT_FIRE_STOP then
      attack_fire_stop(x,y)
    else
      attack_fire(x,y)
    end
  elseif info.attack_type == 2 then
    attack_melee(x,y)
  elseif info.attack_type == 1 then
    attack_reach(x,y)
  elseif info.attack_type == -1 then
    crawl.mpr("No reachable target in view!")
  elseif allow_movement then
    if not AUTOFIGHT_PROMPT_RANGE or crawl.weapon_check() then
      move_towards(x,y)
    end
  elseif AUTOFIGHT_WAIT then
    crawl.do_commands({"CMD_WAIT"})
  else
    crawl.mpr("No target in range!")
  end
end

function hit_closest()
  if AUTOMAGIC_ACTIVE and you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(true)
  else
    attack(true)
  end
end

function hit_closest_nomove()
  if AUTOMAGIC_ACTIVE and you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(false)
  else
    attack(false)
  end
end

function hit_nonmagic()
  attack(true)
end

function hit_nonmagic_nomove()
  attack(false)
end

function hit_magic()
  if you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(true)
  else
    crawl.mpr("No spell in slot " .. AUTOMAGIC_SPELL_SLOT .. "!")
  end
end

function hit_magic_nomove()
  if you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(false)
  else
    crawl.mpr("No spell in slot " .. AUTOMAGIC_SPELL_SLOT .. "!")
  end
end

function toggle_autothrow()
  AUTOFIGHT_THROW = not AUTOFIGHT_THROW
  crawl.mpr(AUTOFIGHT_THROW and "Enabling autothrow." or "Disabling autothrow.")
end

chk_lua_option.autofight_stop = set_stop_level
chk_lua_option.autofight_hunger_stop = set_hunger_stop_level
chk_lua_option.autofight_hunger_stop_undead = set_hunger_stop_undead
chk_lua_option.autofight_caught = set_af_caught
chk_lua_option.autofight_throw = set_af_throw
chk_lua_option.autofight_throw_nomove = set_af_throw_nomove
chk_lua_option.autofight_fire_stop = set_af_fire_stop
chk_lua_option.autofight_wait = set_af_wait
chk_lua_option.autofight_prompt_range = set_af_prompt_range
chk_lua_option.automagic_enable = set_automagic
