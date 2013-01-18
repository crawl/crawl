------------------------------------------------------------------------------
-- v_main.lua:
--
-- Main include for Vaults.
--
-- Code by mumra
-- Based on work by infiniplex, based on original work and designs by mumra :)
--
-- Notes:
-- The original had the entire layout code in layout_vaults.des under a
-- single map def. This made things very hard to understand. Here I am
-- separating things into some separate functions which can be called
-- from short map header in layout_vaults.des. This at least makes it
-- easier to control the weightings and depths of those layouts, and to
-- add new layouts.
------------------------------------------------------------------------------

require("dlua/v_main.lua")

function build_vaults_ring_level(e)
    if not crawl.game_started() then return end

    local gxm, gym = dgn.max_bounds()
--    e.extend_map{width = gxm, height = gym}
    e.layout_type "vaults_ring"

    -- Fill whole dungeon with stone
    dgn.fill_grd_area(0, 0, gxm - 1, gym - 1, "stone_wall")

    -- Initialise glyphs
    local floor = '.'
    local walk_wall = crawl.random_element { ['v'] = 6, ['c'] = 4, ['b'] = 1 }
    local vault_wall = crawl.random_element { ['v'] = 6, ['c'] = 4, ['b'] = 1 }

    -- Create the walkway
    local padMin,padMax,walkMin,walkMax,roomsOuter,roomsInner = 15,20,3,8,true,true

    -- Outer padding
    local paddingX = crawl.random_range(padMin,padMax) --crawl.random_range(12,16)
    local paddingY = crawl.random_range(padMin,padMax) --crawl.random_range(12,16)

    -- Widths of walkways
    local walkWidth1 = crawl.random_range(walkMin,walkMax)
    local walkWidth2 = crawl.random_range(walkMin,walkMax)
    local walkHeight1 = crawl.random_range(walkMin,walkMax)
    local walkHeight2 = crawl.random_range(walkMin,walkMax)

    local walkX11 = paddingX
    local walkX21 = gxm - paddingX
    local walkX12 = paddingX + walkWidth1
    local walkX22 = gxm - paddingX - walkWidth2
    local walkY11 = paddingY
    local walkY21 = gym - paddingY
    local walkY12 = paddingY + walkHeight1
    local walkY22 = gym - paddingY - walkHeight2

  --  dgn.fill_grd_area(1, 1, gxm - 2, gym - 2, "stone_wall")

--    e.fill_area{x1=walkX11, y1=walkY11, x2=walkX21, y2=walkY21, fill=floor }
  --  e.fill_area{x1=walkX12, y1=walkY12, x2=walkX22, y2=walkY22, fill='x'}

    dgn.fill_grd_area(walkX11, walkY11, walkX21, walkY21, "floor")
    dgn.fill_grd_area(walkX12, walkY12, walkX22, walkY22, "stone_wall")

    if crawl.one_chance_in(4) then
      -- TODO: Flood a corridor patch with water
    end

    -- Populate walls with vaults
    local eligible_walls = {
      { walkX11,walkY11,walkX21-walkX11+1,{x=0,y=-1},1 }, -- North outer
      { walkX21,walkY11,walkY21-walkY11+1,{x=1,y=0},1  }, -- East outer
      { walkX21,walkY21,walkX21-walkX11+1,{x=0,y=1},1  }, -- South outer
      { walkX11,walkY21,walkY21-walkY11+1,{x=-1,y=0},1  }, -- West outer

      { walkX22,walkY12-1, walkX22-walkX12+1,{x=0,y=1},1 }, -- North inner
      { walkX22+1,walkY22, walkY22-walkY12+1,{x=-1,y=0},1 }, -- East inner
      { walkX12,walkY22+1, walkX22-walkX12+1,{x=0,y=-1},1 }, -- South inner
      { walkX12-1,walkY12, walkY22-walkY12+1,{x=1,y=0},1 } -- West inner
    }
    return eligible_walls

end

function place_vaults_rooms(e,data)

    -- Figure out what rooms we're using
    -- TODO: This needs to vary more depending on the layout
    local room_spread = {
      { "upstairs", 2, 3 },
      { "downstairs", 3, 3 },
--      { "encounter", 1,4 },
      { "special", 1,2 },
      { "loot", 0, 2 },
      { "empty", 7, 10 },
    }
    -- Manually adjust frequencies of stair rooms
    -- Always have 3 downstairs rooms in V:4
    -- TODO: Still not perfect, need some more logic. This means 3 stair *rooms* will
    --       always be placed in V:4 even if one already had 3 stair cases. See st_'s
    --       comments on V:4 on the tracker.
    if you.depth < dgn.br_depth(you.branch()) - 1 then
      if crawl.one_chance_in(4) then
        room_spread[1] = { "downstairs", 2, 3 }
      end
      if crawl.one_chance_in(6) then
        room_spread[1] = { "downstairs", 1, 3 }
      end
      if crawl.one_chance_in(6) then
        room_spread[2] = { "upstairs", 1, 3 }
      end

    -- Min 10 rooms, max 20
    local room_queue = { }
    for i,spread in ipairs(room_spread) do
      local room_count = crawl.random_range(spread[2],spread[3])
      for n = 0,room_count-1 do
          table.insert(room_queue,spread[1])
      end
    end
    -- TODO: Shuffle queue because rooms to the end are increasingly unlikely to place
    --       (but always keep stairs first, they're kind of important)

    -- Finally, attempt to place all items in the queue
    for i,queued in ipairs(room_queue) do
      place_vaults_room(e,data,queued)
    end
end

function place_vaults_room(e,walls,room_type)
      local maxTries = 50
      local tries = 0
      local done = false
      -- Have a few goes, a random wall spot will be selected each time from all available walls
      while tries < maxTries and not done do
        tries = tries + 1
        local which_wall = eligible_walls[crawl.random_range(1,table.count(eligible_walls))]
        -- Attempt to place a room on the wall at a random position
        if vaults_maybe_place_vault(e, which_wall, queued, crawl.random_range(2,which_wall[3]-3), walls) then
          done = true
        end
      end
end

-- Moves from a start point along a move vector mapped to actual vectors xVector/yVector
-- This allows us to deal with coords independently of how they are rotated to the dungeon grid
local function vaults_vector_add(start, move, xVector, yVector)

  return {
    x = start.x + move.x * xVector.x + move.y * xVector.y,
    y = start.y + move.x * yVector.x + move.y * yVector.y
  }

end

-- Rotates count * 90 degrees anticlockwise
-- Highly rudimentary
local function vector_rotate(vec, count)
  if count > 0 then
    local rotated = { x = -vec.y, y = vec.x }
    count = count - 1
    if count <= 0 then return rotated
    return vector_rotate(vec,count)
  end
  if count < 0 then
    local rotated = { x = vec.y, y = -vec.x }
    count += 1
    if count >= 0 then return rotated
    return vector_rotate(vec,count)
  end
  return vec
end

local function vaults_maybe_place_vault(e, wall, room_type, distance, walls, max_room_depth)

    -- Resolve a map with the specified tag
    local map, vplace = dgn.resolve_map(dgn.map_by_tag("vaults_room_"..room_type),true,true)
    local room_width,room_height
    if map ~= nil then
      room_width,room_height = dgn.mapsize(map)
--           print( "Size " .. room_width .. " x " .. room_height)
    else
      return false
    end

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  local orient = wall[4]
  local door_vector = vector_rotate(orient, 1)

-- print("Vectors: " .. orient.x .. ", " .. orient.y .. " and " .. door_vector.x .. ", "..door_vector.y)

  local wall_base = { x = wall[1], y = wall[2] }
  local target = vaults_vector_add(wall_base, { x = distance, y = 0 }, door_vector, orient)

  -- How big the door? TODO: Choose later (let vault tags veto door sizes, in fact vault tags should allow the room to position its own door/wall/windows)
  local door_length = crawl.random_range(2,4)
  local has_windows = crawl.one_chance_in(5)

--      local oriented = string.find(dgn.tags(map), " no_rotate ")
  -- TODO: Could just check orient flag of map?
--      crawl.mpr("Width " .. room_width .. " height " .. room_height)

  local relative_to_door = math.floor((room_width - door_length)/2) + crawl.random_range(-1,1) --  0 -- crawl.random_range(0,(room_width-door_length)/2)
  local relative = { x = target.x - door_vector.x * relative_to_door, y = target.y - door_vector.y * relative_to_door }

  local origin = { x = -1, y = -1 }
  local opposite = { x = -1, y = -1 }
  local is_clear = true
  local grid = dgn.grid

  -- Check every square this room will go, including its walls, make sure we have space
  for rx = -1, room_width, 1 do
    for ry = 1, room_height + 2, 1 do
      local dx = relative.x + (rx * door_vector.x) + (ry * orient.x)
      local dy = relative.y + (rx * door_vector.y) + (ry * orient.y)

      -- Store origin (top-left corner) for map placement (not 100% certain if this is how the placement coords work tho...)
      if ((rx == 0 or rx == (room_width-1)) and (ry == 2 or ry == (room_height+1))) then
        if (origin.x == -1 or dx < origin.x) then origin.x = dx end
        if (origin.y == -1 or dy < origin.y) then origin.y = dy end
        if (opposite.x == -1 or dx > opposite.x) then opposite.x = dx end
        if (opposite.y == -1 or dy > opposite.y) then opposite.y = dy end
      end

      -- Check dx,dy are uninterrupted
      if not feat.is_wall(grid(dx,dy)) then
        is_clear = false
        break
      end

    end

    if not is_clear then
      break
    end
  end

  if not is_clear then
    return false
  end

  -- Now need the door
  local rx1 = relative_to_door
  local dx1 = relative.x + (rx1 * door_vector.x) + (1 * orient.x)
  local dy1 = relative.y + (rx1 * door_vector.y) + (1 * orient.y)
  local rx2 = rx1 + door_length - 1
  local dx2 = relative.x + (rx2 * door_vector.x) + (1 * orient.x)
  local dy2 = relative.y + (rx2 * door_vector.y) + (1 * orient.y)

  -- Fill door
  dgn.fill_grd_area(origin.x, origin.y, opposite.x, opposite.y, "floor")
  dgn.fill_grd_area(dx1, dy1, dx2, dy2, "closed_door")

  -- Place the map inside
  dgn.reuse_map(vplace,origin.x,origin.y,false,false)

  -- Finally ... we can add this room's walls into the walls collection, if we are allowed deeper rooms
  if max_room_depth == 0 or wall[5] < max_room_depth then
    walla = { x = -1,y = 1 }
    wallb = { x = -1, y = room_height+2 }
    wallc = { x = room_width, y = room_height+2 }

    wallao = vaults_vector_add(relative, walla, door_vector, orient)
    wallab = vaults_vector_add(relative, wallb, door_vector, orient)
    wallac = vaults_vector_add(relative, wallc, door_vector, orient)

    -- Left wall
    table.insert(walls, { wallao.x, wallao.y, room_height+2, vector_rotate(orient,1), wall[5]+1 })
    -- Read wall
    table.insert(walls, { wallbo.x, wallbo.y, room_height+2, orient, wall[5]+1 })
    -- Right wall
    table.insert(walls, { wallco.x, wallco.y, room_height+2, vector_rotate(orient,-1), wall[5]+1 })

  end

  -- TODO: We'll might want to get some data from the placed map e.g. tags and return it
  return true
end
