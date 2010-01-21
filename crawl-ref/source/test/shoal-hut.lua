-- Generates lots of Shoal:$ maps and tests that a) all huts are
-- connected to the exterior b) the stairs are not completely surrounded by
-- deep water.

local iterations = 1000

local isdoor = dgn.feature_set_fn("closed_door", "open_door", "secret_door")
local floor = dgn.fnum("floor")

local function find_vault_doors(vault)
  local doors = { }
  local size = vault:size()
  if size.x == 0 and size.y == 0 then
    return doors
  end
  for p in iter.rect_size_iterator(vault:pos(), size) do
    local thing = dgn.grid(p.x, p.y)
    if isdoor(thing) then
      table.insert(doors, p)
    end
  end
  return doors
end

local function shoal_hut_doors()
  local maps = dgn.maps_used_here()
  test.map_assert(#maps > 0, "No maps used on Shoal:$?")
  local doors = { }
  for _, vault in ipairs(maps) do
    -- Sweep the vault looking for (secret) doors.
    local vault_doors = find_vault_doors(vault)
    if #vault_doors > 0 then
      table.insert(doors, vault_doors)
    end
  end
  test.map_assert(#doors > 0, "No hut doors found on Shoal:$")
  return doors
end

-- The hut door is blocked if we cannot move from the door to a non-vault
-- square without crossing solid terrain.
local function hut_door_blocked(door)
  local function good_square(p)
    return not dgn.in_vault(p.x, p.y)
  end
  local function passable_square(p)
    return not feat.is_solid(dgn.grid(p.x, p.y))
  end
  return not dgn.find_adjacent_point(door, good_square, passable_square)
end

local function verify_stair_connected(p)
  local function good_square(c)
    return dgn.grid(c.x, c.y) == floor
  end
  local function traversable_square(c)
    local dfeat = dgn.grid(c.x, c.y)
    return dfeat == floor or feat.is_stone_stair(dfeat)
  end
  test.map_assert(dgn.find_adjacent_point(p, good_square, traversable_square),
                  "Stairs not connected at " .. p)
end

local function verify_stair_connectivity()
  local function is_stair(p)
    return feat.is_stone_stair(dgn.grid(p.x, p.y))
  end

  local stair_pos = dgn.find_points(is_stair)
  test.map_assert(#stair_pos > 0, "No stairs in map?")

  for _, stair in ipairs(stair_pos) do
    verify_stair_connected(stair)
  end
end

local function verify_hut_connectivity()
  for _, vault_doors in ipairs(shoal_hut_doors()) do
    if util.forall(vault_doors, hut_door_blocked) then
      for _, door in ipairs(vault_doors) do
        dgn.grid(door.x, door.y, "floor_special")
      end
      test.map_assert(false, "Shoal hut doors blocked")
    end
  end
end

local function test_shoal_huts(nlevels)
  debug.goto_place("Shoal:$")
  for i = 1, nlevels do
    crawl.mesclr()
    crawl.mpr("Shoal test " .. i .. " of " .. nlevels)
    test.regenerate_level()
    verify_stair_connectivity()
    verify_hut_connectivity()
  end
end

test_shoal_huts(iterations)
