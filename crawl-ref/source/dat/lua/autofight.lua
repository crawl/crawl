---------------------------------------------------------------------------
-- autofight.lua:
-- One-key fighting.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/autofight.lua
--
-- This uses the very incomplete client monster and view bindings, and
-- is currently very primitive. Improvements welcome!
---------------------------------------------------------------------------

local ATT_HOSTILE = 0
local ATT_NEUTRAL = 1

AUTOFIGHT_STOP = 30

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

local function get_monster_info(dx,dy)
  m = monster.get_monster_at(dx,dy)
  if not m then
    return nil
  end
  info = {}
  info.distance = (abs(dx) > abs(dy)) and -abs(dx) or -abs(dy)
  if have_ranged() then
    info.can_hit = you.see_cell_no_trans(dx, dy) and 3 or 0
  elseif not have_reaching() then
    info.can_hit = (-info.distance < 2) and 2 or 0
  else
    if -info.distance > 2 then
      info.can_hit = 0
    elseif -info.distance < 2 then
      info.can_hit = 2
    else
      info.can_hit = 1
    end
  end
  info.safe = m:is_safe() and -1 or 0
  -- Only prioritize good stabs: sleep and paralysis.
  info.very_stabbable = m:is_very_stabbable() and 1 or 0
  info.injury = m:damage_level()
  info.threat = m:threat()
  return info
end

local function compare_monster_info(m1, m2)
  flag_order = {"safe", "distance", "very_stabbable", "injury", "threat"}
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
  --if m then crawl.mpr("Checking: (" .. x .. "," .. y .. ") " .. m:desc()) end
  if not m or m:attitude() ~= ATT_HOSTILE then
    return false
  end
  if string.find(m:desc(), "butterfly")
      or string.find(m:desc(), "orb of destruction") then
    return false
  end
  if m:is_firewood() then
  --crawl.mpr("... is firewood.")
    if string.find(m:desc(), "ballistomycete") then
      return true
    end
    return false
  end
  return true
end

local function get_target()
  local x, y, bestx, besty, best_info, new_info
  bestx = 0
  besty = 0
  best_info = nil
  for x = -8,8 do
    for y = -8,8 do
      if is_candidate_for_attack(x, y) then
        new_info = get_monster_info(x, y)
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

local function attack_reach(x,y)
  move = 'vr' .. vector_move(x, y) .. '.'
  crawl.process_keys(move)
end

local function attack_melee(x,y)
  move = delta_to_vi(x, y)
  crawl.process_keys(move)
end

local function set_stop_level(key, value)
  AUTOFIGHT_STOP = tonumber(value)
end

local function hp_is_low()
  local hp, mhp = you.hp()
  return (100*hp <= AUTOFIGHT_STOP*mhp)
end

function attack(allow_movement)
  local x, y, info = get_target()
  local caught = you.caught()
  if you.confused() then
    crawl.mpr("You are too confused!")
  elseif caught then
    crawl.mpr("You are " .. caught .. "!")
  elseif hp_is_low() then
    crawl.mpr("You are too injured to fight blindly!")
  elseif info == nil then
    crawl.mpr("No target in view!")
  elseif info.can_hit == 3 then
    attack_fire(x,y)
  elseif info.can_hit == 2 then
    attack_melee(x,y)
  elseif info.can_hit == 1 then
    attack_reach(x,y)
  elseif allow_movement then
    move_towards(x,y)
  else
    crawl.mpr("No target in range!")
  end
end

function hit_closest()
  attack(true)
end

function hit_adjacent()
  attack(false)
end

chk_lua_option.autofight_stop = set_stop_level
