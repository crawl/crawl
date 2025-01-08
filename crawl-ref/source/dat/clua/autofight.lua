---------------------------------------------------------------------------
-- autofight.lua:
-- One-key fighting.
--
-- To use these, it is *not* recommended to bind a key directly to the lua
-- functions, except for toggle_autothrow. Rather, bind a key to the associated
-- commands.
--
-- CMD_AUTOFIGHT (===hit_closest), tab on the default binding.
-- With autofight_throw = false: Attack with primary weapon, moving towards
--     enemy if needed. If the primary weapon is ranged, it will fire. Default.
-- With autofight_throw = true: Attack with primary weapon if within range.
--     Otherwise, fire quiver action if possible; only actions that do direct
--     damage will be considered. If no ranged attack is available, fall back on
--     movement. If a launcher is wielded, prioritize the launcher over the
--     quiver.
--
-- ===toggle_autothrow, not bound by default
-- toggle autofight_throw
--
-- CMD_AUTOFIRE (===hit_closest), shift-tab on the default binding
-- Fire the quiver action, including actions that don't do direct damage; never
-- moves.
--
-- CMD_AUTOFIGHT_NOMOVE (===hit_closest_nomove), not bound by default
-- Attack with primary weapon if within range, fire the quiver action if not;
-- never moves. Only fires quiver actions that do direct damage. If a launcher
-- is wielded, prioritize the launcher over the quiver.
--
-- This uses the very incomplete client monster and view bindings, and
-- is currently very primitive. Improvements welcome!
---------------------------------------------------------------------------

local ATT_HOSTILE = 0
local ATT_NEUTRAL = 1


AUTOFIGHT_STOP = 50
AUTOFIGHT_CAUGHT = false
AUTOFIGHT_THROW = false
AUTOFIGHT_THROW_NOMOVE = true
AUTOFIGHT_FIRE_STOP = false
AUTOFIGHT_WAIT = false
AUTOFIGHT_PROMPT_RANGE = true
AUTOMAGIC_ACTIVE = false
AUTOFIGHT_FORCE_FIRE = false

local function delta_to_cmd(dx, dy)
  local d2v = {
    [-1] = { [-1] = "CMD_MOVE_UP_LEFT",  [0] = "CMD_MOVE_LEFT",  [1] = "CMD_MOVE_DOWN_LEFT"},
    [0]  = { [-1] = "CMD_MOVE_UP",                               [1] = "CMD_MOVE_DOWN"},
    [1]  = { [-1] = "CMD_MOVE_UP_RIGHT", [0] = "CMD_MOVE_RIGHT", [1] = "CMD_MOVE_DOWN_RIGHT"},
  }
  return d2v[dx][dy]
end

local function target_delta_to_cmd(dx, dy)
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
    a[#a+1] = target_delta_to_cmd(sign(dx), 0)
  end
  for i = 1,abs(dy) do
    a[#a+1] = target_delta_to_cmd(0, sign(dy))
  end
end

local function reach_range()
  local r = 1
  local wp = items.equipped_at("weapon")
  if wp and not wp.is_melded then
      r = wp.reach_range
  end
  local o = items.equipped_at("shield")
  if o and not o.is_melded and o.is_weapon and o.reach_range > r then
      r = o.reach_range
  end
  return r
end

local function have_reaching()
  return reach_range() > 1
end

local function have_ranged()
  local wp = items.equipped_at("weapon")
  return wp and wp.is_ranged and not wp.is_melded
end

local function have_quiver_action(no_move)
  return ((AUTOFIGHT_THROW or no_move and AUTOFIGHT_THROW_NOMOVE)
          and you.quiver_valid(1) and you.quiver_enabled(1)
          -- TODO: armataur roll passes the following check, which may be
          -- counterintuitive for the nomove case
          and you.quiver_allows_autofight()
          and (not you.quiver_uses_mp() or not AUTOMAGIC_FIGHT or not af_mp_is_low()))
end

local function is_safe_square(dx, dy)
    if view.feature_at(dx, dy) == "trap_web" and not you.is_web_immune() then
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
  local los_radius = you.los()
  local move = nil
  local dx = bx - ax
  local dy = by - ay
  local function try_move(mx, my)
    if mx == 0 and my == 0 then
      return nil
    elseif abs(ax+mx) > los_radius or abs(ay+my) > los_radius then
      return nil
    elseif not view.cell_see_cell(ax + mx, ay + my, ax + dx, ay + dy) then
      -- we know that dx,dy is currently in view, so if there is a path at all,
      -- there must be a path where it remains in view.
      return nil
    elseif square_func(ax+mx, ay+my) then
      return {mx,my}
    else
      return nil
    end
  end
  if abs(dx) > abs(dy) then
    if abs(dy) == 1 then
      -- at distance one, there's no need to adjust y position
      move = try_move(sign(dx), 0)
    end
    -- first try diagonal. Not sure why? Also, sign(0)=0, so these two checks
    -- are equivalent in that case.
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
    -- exact diagonal
    move = try_move(sign(dx), sign(dy))
    if move == nil then move = try_move(sign(dx), 0) end
    if move == nil then move = try_move(0, sign(dy)) end
  else
    -- symmetric case to first condition, i.e. abs(dy) > abs(dx)
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
    if you.status("immotile") then
      crawl.do_commands({"CMD_WAIT"})
    else
      crawl.do_commands({delta_to_cmd(move[1],move[2])})
    end
  end
end

local function will_tab(ax, ay, bx, by)
  local range = reach_range()
  if abs(bx-ax) <= range and abs(by-ay) <= range then
    return true
  end
  local move = choose_move_towards(ax, ay, bx, by, can_move_maybe)
  if move == nil then
    return false
  end
  return will_tab(ax+move[1], ay+move[2], bx, by)
end

-- attack types for get_monster_info return value
local AF_FAILS = -1
local AF_MOVES = 0    -- target not in range, can move towards it
local AF_REACHING = 1 -- target in range for a reaching attack + reaching available
local AF_MELEE = 2    -- target in melee range + melee attack available
local AF_FIRE = 3     -- target in fire range + ranged attack available

local function get_monster_info(dx,dy,no_move)
  m = monster.get_monster_at(dx,dy)
  name = m:name()
  if not m then
    return nil
  end
  info = {}
  info.distance = (abs(dx) > abs(dy)) and -abs(dx) or -abs(dy)
  if have_ranged() then
    info.attack_type = you.see_cell_no_trans(dx, dy) and AF_FIRE or AF_MOVES
  elseif not have_reaching() then
    info.attack_type = (-info.distance < 2) and AF_MELEE or AF_MOVES
  else
    local range = reach_range()
    -- Assume extended reach (i.e. Rift) gets smite targeting.
    local can_reach = range > 2 and you.see_cell_no_trans or view.can_reach
    if -info.distance > range then
      info.attack_type = AF_MOVES
    elseif -info.distance < 2 then
      info.attack_type = AF_MELEE
    else
      info.attack_type = can_reach(dx, dy) and AF_REACHING or AF_MOVES
    end
  end
  if info.attack_type == 0 and have_quiver_action(no_move) and you.see_cell_no_trans(dx, dy) then
    info.attack_type = AF_FIRE
  end
  if info.attack_type ~= AF_FIRE and AUTOFIGHT_FORCE_FIRE then
    -- firing can often be preempted by melee etc, but we have been called by
    -- CMD_AUTOFIRE, so force firing.
    -- TODO: refactor so that this is less hacky
    info.attack_type = AF_FIRE
  end

  if info.attack_type == AF_MOVES and not will_tab(0,0,dx,dy) then
    info.attack_type = AF_FAILS
  end
  info.can_attack = (info.attack_type > 0) and 1 or info.attack_type
  info.safe = m:is_safe() and -1 or 0
  info.constricting_you = m:is_constricting_you() and 1 or 0
  -- Only prioritize top-tier stabs: sleep, petrification, and paralysis.
  info.very_stabbable = (m:stabbability() >= 1) and 1 or 0
  info.injury = m:damage_level()
  info.threat = m:threat()
  info.orc_priest_wizard = (name == "orc priest" or name == "orc wizard") and 1 or 0
  info.bullseye_target = (info.attack_type == AF_FIRE and m:status("targeted by your dimensional bullseye")) and -1 or 0
  return info
end

local function compare_monster_info(m1, m2)
  flag_order = autofight_flag_order
  if flag_order == nil then
    flag_order = {"bullseye_target", "can_attack", "safe", "distance", "constricting_you", "very_stabbable", "injury", "threat", "orc_priest_wizard"}
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
  if not m then
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
  if m:attitude() == ATT_HOSTILE
      or m:attitude() == ATT_NEUTRAL and m:is("frenzied") then
    return true
  end
  return false
end

local function get_target(no_move)
  local los_radius = you.los()
  local x, y, bestx, besty, best_info, new_info
  bestx = 0
  besty = 0
  best_info = nil
  for x = -los_radius,los_radius do
    for y = -los_radius,los_radius do
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
  if AUTOFIGHT_FORCE_FIRE or not have_ranged() then
    -- fire from quiver
    crawl.do_targeted_command("CMD_FIRE", x, y, AUTOFIGHT_FIRE_STOP)
  else
    -- fire a wielded launcher
    crawl.do_targeted_command("CMD_PRIMARY_ATTACK", x, y, AUTOFIGHT_FIRE_STOP)
  end
end

local function attack_reach(x,y)
  crawl.do_targeted_command("CMD_PRIMARY_ATTACK", x, y, true)
end

local function attack_melee(x,y)
  crawl.do_commands({delta_to_cmd(x, y)})
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

-- is there a better way to get lua options from c++ than defining a function?
function get_af_fire_stop()
  return AUTOFIGHT_FIRE_STOP
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

function af_mp_is_low()
  local mp, mmp
  if you.race() == "Djinni" then
    mp, mmp = you.hp()
  else
    mp, mmp = you.mp()
  end
  -- AUTOMAGIC_STOP is currently in automagic.lua
  return (100*mp <= AUTOMAGIC_STOP*mmp)
end

function autofight_check_preconditions(check_caught)
  local caught = you.caught()
  if af_hp_is_low() then
    crawl.mpr("You are too injured to fight recklessly!")
    return false
  elseif you.confused() then
    crawl.mpr("You are too confused!")
    return false
  elseif caught and check_caught and not AUTOFIGHT_CAUGHT then
    crawl.mpr("You are " .. caught .. "!")
    return false
  end
  return true
end

function attack(allow_movement, check_caught)
  local x, y, info = get_target(not allow_movement)
  if not autofight_check_preconditions(check_caught) then
    return
  end

  if check_caught and you.caught() then
    crawl.do_commands({delta_to_cmd(1, 0)}) -- Direction doesn't matter.
    return
  end

  if info == nil then
    if AUTOFIGHT_WAIT and not allow_movement then
      crawl.do_commands({"CMD_WAIT"})
    else
      crawl.mpr("No target in view!")
    end
  elseif info.attack_type == AF_FIRE then
    attack_fire(x,y)
  elseif info.attack_type == AF_MELEE then
    attack_melee(x,y)
  elseif info.attack_type == AF_REACHING then
    attack_reach(x,y)
  elseif info.attack_type == AF_FAILS then
    crawl.mpr("No reachable target in view!")
  elseif allow_movement then
    if not AUTOFIGHT_PROMPT_RANGE or you.quiver_valid(0) or crawl.weapon_check() then
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
    attack(true, true)
  end
end

function hit_closest_nomove()
  if AUTOMAGIC_ACTIVE and you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(false)
  else
    attack(false, true)
  end
end

function fire_closest()
  if AUTOMAGIC_ACTIVE and you.spell_table()[AUTOMAGIC_SPELL_SLOT] then
    mag_attack(false)
  else
    local old = AUTOFIGHT_FORCE_FIRE
    AUTOFIGHT_FORCE_FIRE = true
    attack(false, false)
    AUTOFIGHT_FORCE_FIRE = old
  end
end

function hit_nonmagic()
  attack(true, true)
end

function hit_nonmagic_nomove()
  attack(false, true)
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
chk_lua_option.autofight_fires = set_af_throw
chk_lua_option.autofight_nomove_fires = set_af_throw_nomove
-- the following two options are here for backwards compatibility
chk_lua_option.autofight_throw = set_af_throw
chk_lua_option.autofight_throw_nomove = set_af_throw_nomove
chk_lua_option.autofight_fire_stop = set_af_fire_stop
chk_lua_option.autofight_wait = set_af_wait
chk_lua_option.autofight_prompt_range = set_af_prompt_range
chk_lua_option.automagic_enable = set_automagic
