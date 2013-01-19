------------------------------------------------------------------------------
-- v_rooms.lua:
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
    if count <= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  if count < 0 then
    local rotated = { x = vec.y, y = -vec.x }
    count = count + 1
    if count >= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  return vec
end

local normals = {
  { x = 0, y = -1, dir = 0 },
  { x = -1, y = 0, dir = 1 },
  { x = 0, y = 1, dir = 2 },
  { x = 1, y = 0, dir = 3 }
}

local function print_vector(caption,v)
  if v == nil then
   print(caption .. ": nil")
  else
    print(caption .. ": " .. v.x .. ", " .. v.y)
  end
end

local function pick_room(e, options)

  local weights = { }

  -- Do we just want an empty room?
  if crawl.random2(100) < options.empty_chance then

    room = {
      type = "empty",
      size = { x = crawl.random_range(options.min_room_size,options.max_room_size), y = crawl.random_range(options.min_room_size,options.max_room_size) }
    }
    return room

  else
    -- Pick by tag
    -- Ignore weights for now
    local room_type = "room"

    local found = false
    local tries = 0
    local maxTries = 20

    while tries < maxTries and not found do
      -- Resolve a map with the specified tag
      local mapdef = dgn.map_by_tag("vaults_"..room_type)
      local map, vplace = dgn.resolve_map(mapdef,true,true)
      local room_width,room_height

      if map == nil then
        -- If we can't find a map then we're probably not going to find one
        return nil
      else
        local room_width,room_height = dgn.mapsize(map)
        local veto = false
        if not (room_width > options.max_room_size or room_width < options.min_room_size or room_height > options.max_room_size or room_height < options.min_room_size) then
          local room = {
            type = "vault",
            size = { x = room_width, y = room_height },
            map = map,
            vplace = vplace
          }
          if options.veto_callback ~= nill then
            local result = options.veto_callback(room)
            if result == true then veto = true end
          end
          if not veto then
            return room
          end
        end
      end
    end
  end
  return nil

end

local function analyse_vault_post_placement(e,usage_grid,room,result,options)

  -- for p in iter.rect_iterator(dgn.point(p1.x, p1.y), dgn.point(p2.x, p2.y)) do

  -- end

end

function place_vaults_rooms(e,data, room_count, options)

  if options == nil then options = { } end

  -- Set default options
  if options.min_room_size == nil then options.min_room_size = 5 end
  if options.max_room_size == nil then options.max_room_size = 20 end
  if options.max_room_depth == nil then options.max_room_depth = 0 end -- No max
  if options.emty_chance == nil then options.empty_chance = 50 end -- Chance in 100

  -- Attempt to place as many rooms as we've been asked for
--  for i = 1, room_count, 1 do
  for i = 1, 10, 1 do
    local placed = false
    local tries = 0
    local maxTries = 50


    local results = { }
    -- Try several times to find a room that passes placement (it could get vetoed at the placement stage,
    -- for instance low-depth rooms might be required to have multiple entrances)
    while tries < maxTries and not placed do
      tries = tries + 1
      -- Pick a room
      local room = pick_room(e, options)
      -- Attempt to place it. The placement function will try several times to find somewhere to fit it.
      if room ~= nil then
        local result = place_vaults_room(e,data,room,options)
        if result.placed then
          placed = true
          if room.type == "vault" then
            -- Perform analysis for stairs
            analyse_vault_post_placement(e,data,room,result,options)
            table.insert(results,result)
          end
        end
      end
    end
  end

  -- TODO: Finally, pick the rooms where we'll place the stairs

  -- Set MMT_VAULT across the whole map depending on usage. This way the dungeon builder knows where to place standard vaults without messing up the layout.
  local gxm, gym = dgn.max_bounds()
  for p in iter.rect_iterator(dgn.point(0, 0), dgn.point(gxm - 1, gym - 1)) do
    local usage = vaults_get_usage(data,p.x,p.y)
    if usage.usage == "restricted" or usage.usage == "eligible_open" or (usage.usage == "eligible" and usage.depth > 1) then
      dgn.set_map_mask(p.x,p.y)
    end
  end

  return true
end

function place_vaults_room(e,usage_grid,room, options)

  local gxm, gym = dgn.max_bounds()

  local maxTries = 50
  local maxSpotTries = 5000
  local tries = 0
  local spotTries = 0
  local done = false
  local foundSpot = false
  local spot = { }
  local usage = { }

  -- Have a few goes at placing; we'll select a random eligible / open spot each time and try to place the vault there
  while tries < maxTries and not done do
    tries = tries + 1

    -- Find an eligible spot
    spotTries = 0
    foundSpot = false
    -- Brute force, this needs optimising, but for now try poll the grid up to 5000 times until we find an eligible spot
    while spotTries < maxSpotTries and not foundSpot do
      spotTries = spotTries + 1
      local spotx = crawl.random_range(0,gxm-1)
      local spoty = crawl.random_range(0,gxm-1)
      spot = { x = spotx, y = spoty }
      usage = vaults_get_usage(usage_grid,spot.x,spot.y)
      print ("Usage: " .. usage.usage)
      if (usage.usage == "eligible" or usage.usage == "open") then
        foundSpot = true
      end
    end

    if foundSpot == true then
      -- Attempt to place the room on the wall at a random position
      if vaults_maybe_place_vault(e, spot, usage_grid, usage, room, options) then
        done = true
      end
    end
  end
  return { placed = done }
end

function vaults_maybe_place_vault(e, pos, usage_grid, usage, room, options)
  local room_width, room_height = room.size.x, room.size.y
  print ("Map size: " .. room_width .. " x " .. room_height)

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  local v_normal, v_wall, room_base, clear_bounds

  -- Placing a room in open space
  if usage.usage == "open" then
    -- Pick a random orientation
    v_normal = normals[crawl.random_range(1,4)]
    print_vector("v_normal",v_normal)
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
    print_vector("v_wall",v_wall)
    -- Ultimately where the bottom-left corner of the room will be placed (relative to pos)
    -- It is offset by 1,1 from the picked spot
    room_base = vaults_vector_add(pos, { x = 1, y = 1 }, v_wall, v_normal)
    print_vector("room_base", room_base)
  end

  -- Placing a room in a wall
  if usage.usage == "eligible" then
    v_normal = usage.normal
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
    -- Ultimately where the bottom-left corner of the room will be placed (relative to pos)
    -- It is offset by half the room width and up by one to make room for door
    room_base = vaults_vector_add(pos, { x = math.floor(-room_width/2), y = 1 }, v_wall, v_normal)
  end

-- print("Vectors: " .. orient.x .. ", " .. orient.y .. " and " .. door_vector.x .. ", "..door_vector.y)

  -- How big the door? TODO: Choose later (let vault tags veto door sizes, in fact vault tags should allow the room to position its own door/wall/windows)
  local door_length = crawl.random_range(2,4)
  local has_windows = crawl.one_chance_in(5)

--      local oriented = string.find(dgn.tags(map), " no_rotate ")
  -- TODO: Could just check orient flag of map?
--      crawl.mpr("Width " .. room_width .. " height " .. room_height)

  -- Door positioning
  -- local relative_to_door = math.floor((room_width - door_length)/2) + crawl.random_range(-1,1) --  0 -- crawl.random_range(0,(room_width-door_length)/2)
  -- local relative = { x = target.x - door_vector.x * relative_to_door, y = target.y - door_vector.y * relative_to_door }

  local origin = { x = -1, y = -1 }
  local opposite = { x = -1, y = -1 }
  local is_clear = true
  local grid = dgn.grid

  -- Check every square this room will go, including its walls, make sure we have space
  for rx = -1, room_width, 1 do
    for ry = -1, room_height, 1 do

      -- Figure out where this target actually lies after rotation
      local target = vaults_vector_add(room_base, { x = rx, y = ry }, v_wall, v_normal )
      -- For placement we have to store the top-left corner of the room in absolute map coords
      if rx >= 0 and rx < room_width and ry >=0 and ry < room_height then
        if (origin.x == -1 or target.x < origin.x) then origin.x = target.x end
        if (origin.y == -1 or target.y < origin.y) then origin.y = target.y end
        if (opposite.x == -1 or target.x > opposite.x) then opposite.x = target.x end
        if (opposite.y == -1 or target.y > opposite.y) then opposite.y = target.y end
      end

      -- Check target usage
      local target_usage = vaults_get_usage(usage_grid,target.x,target.y)
      -- If any square is restricted then we fail. If our room type is "open" then we need to find all open or eligible_open squares (meaning we take up
      -- open space or we are attaching to an existing building). If our room type is "eligible" then we need to find all unused or eligible squares (meaning
      -- we are building into empty walls or attaching to more buildings).
      if (target_usage.usage == "restricted" or (usage.usage == "open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")
        or (usage.usage == "eligible" and target_usage.usage ~= "eligible" and target_usage.usage ~= "none")) then
        is_clear = false
        break
      end

    end

    if not is_clear then
      break
    end
  end
  -- No clear space found, cancel
  if not is_clear then return false end

  print_vector("Origin",origin)
  print_vector("Opposite",opposite)
  print_vector("Pos",pos)

  -- Now need the door
  -- local rx1 = relative_to_door
  -- local dx1 = relative.x + (rx1 * door_vector.x) + (1 * orient.x)
  -- local dy1 = relative.y + (rx1 * door_vector.y) + (1 * orient.y)
  -- local rx2 = rx1 + door_length - 1
  -- local dx2 = relative.x + (rx2 * door_vector.x) + (1 * orient.x)
  -- local dy2 = relative.y + (rx2 * door_vector.y) + (1 * orient.y)

  -- If placing in open space, we need to surround with walls
  -- TODO: Randomly vary the wall type
  if usage.usage == "open" then
    dgn.fill_grd_area(origin.x - 1, origin.y - 1, opposite.x + 1, opposite.y + 1, "stone_wall")
  end

  -- Create floor space to place the vault
  dgn.fill_grd_area(origin.x, origin.y, opposite.x, opposite.y, "floor")
  -- Fill the door
  dgn.fill_grd_area(pos.x, pos.y, pos.x, pos.y, "closed_door")
  -- dgn.fill_grd_area(dx1, dy1, dx2, dy2, "closed_door")

  -- Place the map inside
  if room.type == "vault" then
    dgn.reuse_map(room.vplace,origin.x,origin.y,false,false)
  end

  -- Loop through all squares and update eligibility
  for x = origin.x, opposite.x, 1 do
    for y = origin.y, opposite.y, 1 do
      -- Restricting further placement to keep things simple. Will need to be more clever about how we treat the walls.
      if room.type == "vault" then
        vaults_set_usage(usage_grid,x,y,{usage = "restricted"})
      end
      -- Empty space
      if room.type == "empty" then
        -- Still restrict walls
        if x == origin.x or x == opposite.x or y == origin.y or y == opposite.y then
          vaults_set_usage(usage_grid,x,y,{usage = "restricted"})
        else
          vaults_set_usage(usage_grid,x,y,{usage = "empty"})
        end
      end
    end
  end

  -- TODO: We'll might want to get some data from the placed map e.g. tags and return it
  return true
end


local function vaults_maybe_place_vault_old(e, wall, room_type, distance, walls, max_room_depth)

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

function place_vaults_room_old(e,walls,room_type)
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

function place_vaults_rooms_old(e,data, room_count, max_room_depth)

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