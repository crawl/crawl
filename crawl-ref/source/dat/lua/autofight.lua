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

local function try_move(dx, dy)
  if view.feature_at(dx, dy) == "floor" then
    return delta_to_vi(dx, dy)
  else
    return ""
  end
end

local function move_towards(dx, dy)
  local move = ""
  if abs(dx) > abs(dy) then
    move = try_move(sign(dx), 0)
    if move == "" then move = try_move(sign(dx), sign(dy)) end
    if move == "" then move = try_move(0, sign(dy)) end
  elseif abs(dx) == abs(dy) then
    move = try_move(sign(dx), sign(dy))
    if move == "" then move = try_move(sign(dx), 0) end
    if move == "" then move = try_move(0, sign(dy)) end
  else
    move = try_move(0, sign(dy))
    if move == "" then move = try_move(sign(dx), sign(dy)) end
    if move == "" then move = try_move(sign(dx), 0) end
  end
  if move == "" then
    crawl.mpr("failed to move towards target")
  else
    crawl.process_keys(move)
  end
end

local function find_next_monster()
  local r, x, y
  for r = 1,8 do
    for x = -r,r do
      for y = -r,r do
        if monster.get_monster_at(x, y) then
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
    crawl.mpr("couldn't find monster")
  else
    move_towards(x, y)
  end
end
