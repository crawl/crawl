---------------------------------------------------------------------------
-- autofight.lua:
-- One-key fighting.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/autofight.lua
-- Then macro any key to "===hit_closest".
--
-- This uses the very incomplete client monster and view bindings, and
-- is currently very primitive. Improvements welcome!
---------------------------------------------------------------------------

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

local function reaching(dx, dy)
  local wp = items.equipped_at("weapon")
  if wp and wp.ego_type == "reaching" then
    return dx*dx + dy*dy <= 8
  else
    return nil
  end
end

local function can_move(feat)
  return travel.feature_traversable(feat)
end

local function try_move(dx, dy)
  local feat = view.feature_at(dx, dy)
  if can_move(feat) then
    return delta_to_vi(dx, dy)
  else
    return nil
  end
end

local function move_towards(dx, dy)
  local move = nil
  if reaching(dx, dy) then
    move = 'vf'
  elseif adjacent(dx, dy) then
    move = delta_to_vi(dx, dy)
  elseif abs(dx) > abs(dy) then
    move = try_move(sign(dx), 0)
    if move == nil then move = try_move(sign(dx), sign(dy)) end
    if move == nil then move = try_move(0, sign(dy)) end
  elseif abs(dx) == abs(dy) then
    move = try_move(sign(dx), sign(dy))
    if move == nil then move = try_move(sign(dx), 0) end
    if move == nil then move = try_move(0, sign(dy)) end
  else
    move = try_move(0, sign(dy))
    if move == nil then move = try_move(sign(dx), sign(dy)) end
    if move == nil then move = try_move(sign(dx), 0) end
  end
  if move == nil then
    crawl.mpr("Failed to move towards target.")
  else
    crawl.process_keys(move)
  end
end

local function find_next_monster()
  local r, x, y
  for r = 1,8 do
    for x = -r,r do
      for y = -r,r do
        m = monster.get_monster_at(x, y)
        if m and not m:is_safe() then
          return x, y
        end
      end
    end
  end
  return 0, 0
end

function hit_closest()
  local x, y = find_next_monster()
  if x == 0 and y == 0 then
    crawl.mpr("No unsafe monster in view!")
  else
    move_towards(x, y)
  end
end

function hit_adjacent()
  local x, y = find_next_monster()
  if x == 0 and y == 0 then
    crawl.mpr("No unsafe monster in view!")
  elseif x*x + y*y > 2 and not reaching(x, y) then
    crawl.mpr("That monster is too far!")
  else
    move_towards(x, y)
  end
end
