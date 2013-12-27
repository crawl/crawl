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

AUTOFIGHT_STOP = 30
AUTOFIGHT_CAUGHT = false
AUTOFIGHT_THROW = false
AUTOFIGHT_THROW_NOMOVE = true
AUTOFIGHT_FIRE_STOP = false
AUTOMAGIC_ACTIVE = false

local function delta_to_vi(dx, dy)
  local d2v = {
    [-1] = { [-1] = 'y', [0] = 'h', [1] = 'b'},
    [0]  = { [-1] = 'k',            [1] = 'j'},
    [1]  = { [-1] = 'u', [0] = 'l', [1] = 'n'},
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

local function vector_move(dx, dy)
  local str = ''
  for i = 1,abs(dx) do
    str = str .. delta_to_vi(sign(dx), 0)
  end
  for i = 1,abs(dy) do
    str = str .. delta_to_vi(0, sign(dy))
  end
  return str
end

local function have_reaching()
  local wp = items.equipped_at("weapon")
  return wp and wp.reach_range == 8 and not wp.is_melded
end

local function have_ranged()
  local wp = items.equipped_at("weapon")
  return wp and wp.is_ranged and not wp.is_melded
end

local function have_throwing(no_move)
  return (AUTOFIGHT_THROW or no_move and AUTOFIGHT_THROW_NOMOVE) and items.fired_item() ~= nil
end

local function try_move(dx, dy)
  m = monster.get_monster_at(dx, dy)
  -- attitude > ATT_NEUTRAL should mean you can push past the monster
  if view.is_safe_square(dx, dy) and (not m or m:attitude() > ATT_NEUTRAL) then
    return delta_to_vi(dx, dy)
  else
    return nil
  end
end

local function move_towards(dx, dy)
  local move = nil
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
  if move == nil then
    crawl.mpr("Failed to move towards target.")
  else
    crawl.process_keys(move)
  end
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
  info.can_attack = (info.attack_type > 0) and 1 or 0
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
  for x = -8,8 do
    for y = -8,8 do
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
  move = 'fr' .. vector_move(x, y) .. 'f'
  crawl.process_keys(move)
end

local function attack_fire_stop(x,y)
  move = 'fr' .. vector_move(x, y) .. '.'
  crawl.process_keys(move)
end

local function attack_reach(x,y)
  move = 'vr' .. vector_move(x, y) .. '.'
  crawl.process_keys(move)
end

local function attack_melee(x,y)
  move = delta_to_vi(x, y)
  crawl.process_keys(move)
end

local function set_stop_level(key, value, mode)
  AUTOFIGHT_STOP = tonumber(value)
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

function set_automagic(key, value, mode)
  AUTOMAGIC_ACTIVE = string.lower(value) ~= "false"
end

function af_hp_is_low()
  local hp, mhp = you.hp()
  return (100*hp <= AUTOFIGHT_STOP*mhp)
end

function attack(allow_movement)
  local x, y, info = get_target(not allow_movement)
  local caught = you.caught()
  if af_hp_is_low() then
    crawl.mpr("You are too injured to fight recklessly!")
  elseif you.confused() then
    crawl.mpr("You are too confused!")
  elseif caught then
    if AUTOFIGHT_CAUGHT then
      crawl.process_keys(delta_to_vi(1, 0)) -- Direction doesn't matter.
    else
      crawl.mpr("You are " .. caught .. "!")
    end
  elseif info == nil then
    crawl.mpr("No target in view!")
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
  elseif allow_movement then
    move_towards(x,y)
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
chk_lua_option.autofight_caught = set_af_caught
chk_lua_option.autofight_throw = set_af_throw
chk_lua_option.autofight_throw_nomove = set_af_throw_nomove
chk_lua_option.autofight_fire_stop = set_af_fire_stop
chk_lua_option.automagic_enable = set_automagic
