---------------------------------------------------------------------------
-- automagic.lua:
-- One-key casting.
--
--
-- This uses the "very incomplete client monster and view bindings" from
-- autofight.lua, and "is currently very primitive."
--
-- The behavior with target selection is kind of quirky, especially with
-- short range spells and dangerous monsters out of range.
---------------------------------------------------------------------------

local ATT_HOSTILE = 0
local ATT_NEUTRAL = 1

if not AUTOMAGIC_SPELL_SLOT then
  initial_slot = true
  AUTOMAGIC_SPELL_SLOT = "a"
end

AUTOMAGIC_STOP = 0
AUTOMAGIC_FIGHT = false

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
  name = m:name()
  if not m then
    return nil
  end
  info = {}
  info.distance = (abs(dx) > abs(dy)) and -abs(dx) or -abs(dy)

  -- Decide what to do for target's range by circleLOS range
  if (dx^2 + dy^2 <= spells.range(you.spell_table()[AUTOMAGIC_SPELL_SLOT])^2+1) then
    -- In range
    info.attack_type = 1
  else
    -- Out of range
    info.attack_type = 2
  end
  info.can_attack = (info.attack_type == 1) and 1 or 0
  info.safe = m:is_safe() and -1 or 0
  info.constricting_you = m:is_constricting_you() and 1 or 0
  info.injury = m:damage_level()
  info.threat = m:threat()
  info.orc_priest_wizard = (name == "orc priest" or name == "orc wizard") and 1 or 0
  return info
end

local function compare_monster_info(m1, m2)
  flag_order = automagic_flag_order
  if flag_order == nil then
    flag_order = {"can_attack", "safe", "distance", "constricting_you", "injury", "threat", "orc_priest_wizard"}
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

local function spell_attack(x,y)
  -- There has already been a valid target check to have gotten this far.
  -- Required magic points have also been checked.
  -- Spells that could hurt you (clouds, fireball) will still trigger "are you
  -- sure" so that safeguard is not bypassed.
  move = 'z' .. AUTOMAGIC_SPELL_SLOT .. 'r' .. vector_move(x, y) .. 'f'
  crawl.process_keys(move)
end

local function set_stop_level(key, value, mode)
  AUTOMAGIC_STOP = tonumber(value)
end

local function set_fight_behavior(key, value, mode)
  AUTOMAGIC_FIGHT = string.lower(value) ~= "false"
end

local function set_automagic_spell_slot(key, value, mode)
  -- need to check for this to make sure we don't overwrite the saved slot
  if initial_slot then
    AUTOMAGIC_SPELL_SLOT = value
  end
end

local function getkey()
  local key
  while true do
    key = crawl.getch()
    if key == 13 then
      return "null"
    end
    if key == 27 then
      return "escape"
    end
    -- Similar check to libutil.h isaalpha(int c) for valid key
    if (key > 96 and key < 122) or (key > 64 and key < 91) then
      local c = string.char(key)
      return c
    else
      return "invalid"
    end
  end
end

local function mp_is_low()
  local mp, mmp = you.mp()
  return (100*mp <= AUTOMAGIC_STOP*mmp)
end

function mag_attack(allow_movement)
  local x, y, info = get_target()
  if af_hp_is_low() then
    crawl.mpr("You are too injured to fight recklessly!")
  elseif you.confused() then
    crawl.mpr("You are too confused!")
  elseif info == nil then
    crawl.mpr("No target in view!")
  elseif spells.mana_cost(you.spell_table()[AUTOMAGIC_SPELL_SLOT]) > you.mp() then
    -- If you want to resort to melee, set AUTOMAGIC_FIGHT to true in rc
    -- First check for enough magic points, then check if below threshold
    if AUTOMAGIC_FIGHT then
      attack(allow_movement)
    else
      crawl.mpr("You don't have enough magic to cast " ..
          you.spell_table()[AUTOMAGIC_SPELL_SLOT] .. "!")
    end
  elseif mp_is_low() then
    if AUTOMAGIC_FIGHT then
      attack(allow_movement)
    else
      crawl.mpr("You are too depleted to cast spells recklessly!")
    end
  elseif info.attack_type == 1 then
    spell_attack(x,y)
  elseif allow_movement then
    move_towards(x,y)
  else
    crawl.mpr("No target in range!")
  end
end

-- Set this as a macro to change which spell is cast, in game!
function am_set_spell()
  crawl.mpr("Which spell slot to assign to automagic? (Enter to disable, Esc to cancel)", "prompt")
  local slot = getkey()
  crawl.mesclr()

  if slot == "escape" then
    crawl.mpr("Cancelled.")
    return false

  elseif slot == "null" then
    crawl.mpr("Deactivated automagic.")
    crawl.setopt("automagic_enable = false")
    return false

  elseif slot == "invalid" then
    crawl.mpr("Invalid spell slot.")
    return false

  else
    message = ""
    -- All conditional checks completed, continue to assign a slot now. If automagic
    -- had been disabled, enable it now.
    if AUTOMAGIC_ACTIVE == false then
      crawl.setopt("automagic_enable = true")
      message = " enabled,"
    end
    AUTOMAGIC_SPELL_SLOT = slot
    spell_name = you.spell_table()[AUTOMAGIC_SPELL_SLOT] or "no spell currently"
    crawl.mpr("Automagic" .. message .. " will cast spell in slot " .. slot .. " (" ..
        spell_name .. ")" .. ".")
    return false

  end
end

local function spell_slot_save()
  local res = ""
  if AUTOMAGIC_SPELL_SLOT then
    res = res .. "AUTOMAGIC_SPELL_SLOT = " .. "'" .. AUTOMAGIC_SPELL_SLOT
              .. "'\n"
  end
  return res
end

table.insert(chk_lua_save, spell_slot_save)

chk_lua_option.automagic_stop = set_stop_level
chk_lua_option.automagic_fight = set_fight_behavior
chk_lua_option.automagic_slot = set_automagic_spell_slot
