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
    if rotated.x == -0 then rotated.x = 0 end
    if rotated.y == -0 then rotated.y = 0 end
    count = count - 1
    if count <= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  if count < 0 then
    local rotated = { x = vec.y, y = -vec.x }
    if rotated.x == -0 then rotated.x = 0 end
    if rotated.y == -0 then rotated.y = 0 end
    count = count + 1
    if count >= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  return vec
end

local normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

function print_vector(caption,v)
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
    local veto = true

    while veto do
      veto = false

      room = {
        type = "empty",
        size = { x = crawl.random_range(options.min_room_size,options.max_room_size), y = crawl.random_range(options.min_room_size,options.max_room_size) }
      }

      if options.veto_room_callback ~= nil then
        local result = options.veto_room_callback(room)
        if result == true then veto = true end
      end

    end
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
      -- Temporarily prevent map getting mirrored / rotated during resolution
      dgn.tags(mapdef, "no_vmirror no_hmirror no_rotate");
      -- Resolve the map so we can find its width / height
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
          if options.veto_room_callback ~= nil then
            local result = options.veto_room_callback(room)
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
  if options.max_room_size == nil then options.max_room_size = 15 end
  if options.max_room_depth == nil then options.max_room_depth = 0 end -- No max
  if options.emty_chance == nil then options.empty_chance = 50 end -- Chance in 100

  -- Attempt to place as many rooms as we've been asked for
  -- for i = 1, 10, 1 do
  for i = 1, room_count, 1 do
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

  -- Useful if things aren't working:
  dump_usage_grid(data)

  return true
end

function dump_usage_grid(usage_grid)

    local gxm,gym = dgn.max_bounds()

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = usage_grid[i][j]
      if cell.usage == "none" then maprow = maprow .. " "
      elseif cell.usage == "restricted" then
        if cell.reason == "vault" then maprow = maprow .. ","
        elseif cell.reason == "door" then maprow = maprow .. "="
        else maprow = maprow .. "!" end
      elseif cell.usage == "empty" then maprow = maprow .. "."
      elseif cell.usage == "eligible" or cell.usage == "eligible_open" then
        -- if cell.depth == nil then maprow = maprow .. "!"
        -- else maprow = maprow .. cell.depth end
        if cell.normal == nil then maprow = maprow .. "!"
        elseif cell.normal.x == -1 then maprow = maprow .. "<"
        elseif cell.normal.x == 1 then maprow = maprow .. ">"
        elseif cell.normal.y == -1 then maprow = maprow .. "^"
        elseif cell.normal.y == 1 then maprow = maprow .. "v" end
      elseif cell.usage == "open" then maprow = maprow .. "_"  -- Easier to see the starting room
      else maprow = maprow .. "?" end
    end
    print (maprow)
  end

end

function dump_usage_grid_pretty(usage_grid)

    local gxm,gym = dgn.max_bounds()

  for i = 0, gym-1, 1 do
    local maprow = ""
    for j = 0, gxm-1, 1 do
      local cell = usage_grid[i][j]
      if cell.usage == "none" then maprow = maprow .. " "
      elseif cell.usage == "restricted" then
        if cell.reason == "vault" then maprow = maprow .. ","
        elseif cell.reason == "door" then maprow = maprow .. "+"
        else maprow = maprow .. "." end
      elseif cell.usage == "empty" then maprow = maprow .. "."
      elseif cell.usage == "eligible" or cell.usage == "eligible_open" then
        if cell.normal == nil then maprow = maprow .. "!"
        elseif cell.normal.x == 0 then maprow = maprow .. "-"
        elseif cell.normal.y == 0 then maprow = maprow .. "|" end
      elseif cell.usage == "open" then maprow = maprow .. "."
      else maprow = maprow .. "?" end
    end
    print (maprow)
  end

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
      local spotx = crawl.random_range(2,gxm-2)
      local spoty = crawl.random_range(2,gym-2)
      spot = { x = spotx, y = spoty }
      usage = vaults_get_usage(usage_grid,spot.x,spot.y)
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

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  local v_normal, v_wall, room_base, clear_bounds, orient, v_normal_dir
  -- Does the room need a specific orientation?
  if room.type == "vault" then
    local orients = { }
    for i, n in ipairs(normals) do
      if dgn.has_tag(room.map,"vaults_orient_" .. n.name) then
        table.insert(orients,n)
      end
    end
    local count = #orients
    if count > 0 then
      orient = orients[crawl.random_range(1,count)]
    else
      -- Pick one of the four orients now; if the room is rectangular we'll need
      -- to swap the width/height anyway
      orient = normals[crawl.random_range(1,#normals)]
    end
    -- Exchange width and height
    if orient.dir % 2 == 1 then room_width, room_height = room_height, room_width end
  end

  -- Placing a room in open space
  if usage.usage == "open" then
    -- Pick a random rotation for the door wall
    -- TODO: We might as well try all four?
    v_normal = normals[crawl.random_range(1,4)]
    v_normal_dir = v_normal.dir
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
    -- Ultimately where the bottom-left corner of the room will be placed (relative to pos)
    -- It is offset by 1,1 from the picked spot
    room_base = vaults_vector_add(pos, { x = 1, y = 1 }, v_wall, v_normal)
  end

  -- Placing a room in a wall
  if usage.usage == "eligible" or usage.usage == "eligible_open" then
    v_normal = usage.normal
    for i,n in ipairs(normals) do
      if n.x == v_normal.x and n.y == v_normal.y then
        v_normal_dir = n.dir
      end
    end
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
    -- Ultimately where the bottom-left corner of the room will be placed (relative to pos)
    -- It is offset by half the room width and up by one to make room for door
    room_base = vaults_vector_add(pos, { x = math.floor(-room_width/2), y = 1 }, v_wall, v_normal)
  end

  -- Can veto room from options
  if options.veto_place_callback ~= nil then
    local result = options.veto_place_callback(usage,room)
    if result == true then return false end
  end

  local origin = { x = -1, y = -1 }
  local opposite = { x = -1, y = -1 }
  local is_clear = true
  local grid = dgn.grid

  -- Check every square this room will go, including its walls, make sure we have space
  local rx1 = -1
  local rx2 = room_width
  local ry1 = -1
  local ry2 = room_height
  -- If we're placing in the open, need to check the surrounding border to avoid butting
  -- right up next to existing rooms
  if usage.usage == "open" or usage.usage == "eligible_open" then
    rx1 = -2
    ry1 = -2
    rx2 = room_width + 1
    ry2 = room_height + 1
  end
  for rx = rx1, rx2, 1 do
    for ry = ry1, ry2, 1 do

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

      -- Checking outer border (only for open rooms)
      -- TODO: This code should probably be simplified with rectangle iterators and a new border iterator
      if (rx == -2 or ry == -2 or rx == (room_width + 1) or ry == (room_height + 1)) then
        -- Here we don't mind most restricted because that could be the inner row of a subvault we're attaching
        -- to. What we're worried about is if there is a wall *next* to our wall which could block doors.
        if (target_usage.usage == "eligible" or target_usage.usage == "eligible_open" or (target_usage.usage == "restricted" and target_usage.reason ~= "vault")) then
          is_clear = false
          break
        end
      -- Standard check.
      -- If any square is restricted then we fail. If our room type is "open" then we need to find all open or eligible_open squares (meaning we take up
      -- open space or we are attaching to an existing building). If our room type is "eligible" then we need to find all unused or eligible squares (meaning
      -- we are building into empty walls or attaching to more buildings).
      elseif (target_usage.usage == "restricted" or (usage.usage == "open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")
        or (usage.usage == "eligible" and target_usage.usage ~= "eligible" and target_usage.usage ~= "none")
        or (usage.usage == "eligible_open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")) then
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

  -- If placing in open space, we need to surround with walls
  -- TODO: Randomly vary the wall type
  if usage.usage == "open" then
    dgn.fill_grd_area(origin.x - 1, origin.y - 1, opposite.x + 1, opposite.y + 1, "stone_wall")
  end

  -- Create floor space to place the vault in
  dgn.fill_grd_area(origin.x, origin.y, opposite.x, opposite.y, "floor")

  -- Place the map inside the room if we have one
  if room.type == "vault" then
    -- Allow map to by flipped and rotated again
    dgn.tags_remove(room.map, "no_vmirror no_hmirror no_rotate")
    -- Hardwiring this for now
    dgn.tags(room.map, "transparent")

    -- Calculate the final orientation we need for the room to match the door
    -- Somewhat discovered by trial and error but it appears to work
    local final_orient = (orient.dir - v_normal_dir + 2) % 4

    -- We can only rotate a map clockwise or anticlockwise. To rotate 180 we just flip on both axes.
    if final_orient == 0 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,0)
    elseif final_orient == 1 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,1)
    elseif final_orient == 2 then dgn.reuse_map(room.vplace,origin.x,origin.y,true,true,0)
    elseif final_orient == 3 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,-1) end

  end

  local needs_door = true
  -- How big the door?
  if needs_door then
    -- Even or odd width
    local oddness = room_width % 2
    -- Work out half the door length
    local door_max = math.floor(room_width / 2)
    local door_half_length = crawl.random_range(1,door_max)
    -- Multiply by two and add one if odd
    local door_length = door_half_length * 2 + oddness

    local door_start = (room_width - door_length) / 2
    local door_c1 = vaults_vector_add(room_base, { x = door_start, y = -1 }, v_wall, v_normal)
    local door_c2 = vaults_vector_add(room_base, { x = room_width - 1 - door_start, y = -1 }, v_wall, v_normal)

    dgn.fill_grd_area(door_c1.x, door_c1.y, door_c2.x, door_c2.y, "closed_door")

    -- Optionally generate windows
    if door_length < room_width and crawl.one_chance_in(5) and (room.map == nil or not dgn.has_tag(room.map,"vaults_no_windows")) then
      local window_pos1 = crawl.random_range(0, door_start - 1)
      local window_pos2 = crawl.random_range(0, door_start - 1)
      local window_1l = vaults_vector_add(room_base, { x = window_pos1, y = -1 }, v_wall, v_normal)
      local window_1r = vaults_vector_add(room_base, { x = room_width - 1 - window_pos1, y = -1 }, v_wall, v_normal)
      local window_2l = vaults_vector_add(room_base, { x = window_pos2, y = -1 }, v_wall, v_normal)
      local window_2r = vaults_vector_add(room_base, { x = room_width - 1 - window_pos2, y = -1 }, v_wall, v_normal)

      dgn.fill_grd_area(window_1l.x, window_1l.y, window_2l.x, window_2l.y, "clear_stone_wall")
      dgn.fill_grd_area(window_1r.x, window_1r.y, window_2r.x, window_2r.y, "clear_stone_wall")
    end

  end

  -- Loop through all squares and update eligibility
  for p in iter.rect_iterator(dgn.point(origin.x,origin.y), dgn.point(opposite.x,opposite.y)) do
    local x,y = p.x,p.y
    -- Restricting further placement to keep things simple. Will need to be more clever about how we treat the walls.
    if room.type == "vault" then
      vaults_set_usage(usage_grid,x,y,{usage = "restricted", reason = "vault" })
    end
    -- Empty space
    if room.type == "empty" then
      -- Still restrict walls
      -- if x == origin.x or x == opposite.x or y == origin.y or y == opposite.y then
        -- vaults_set_usage(usage_grid,x,y,{usage = "restricted"})
      -- else

      -- Setting usage as "empty" means the empty space will be available for vault placement by the dungeon builder
      vaults_set_usage(usage_grid,x,y,{usage = "empty"})
      -- end
    end
  end

  -- Handle wall usage. We don't set the corners, only adjacent walls, because it's fine for other walls to use the corners
  local wall_usage = "eligible"
  if usage.usage == "open" or usage.usage == "eligible_open" then wall_usage = "eligible_open" end

  -- TODO: Need to check which walls allow attachment (this data needs to be generated in pick_room from orient_* tags)
  -- TODO: Additionally, if a wall overlaps with another eligible wall (that supports an exit) then we should carve a new door.
  -- TODO: Furthermore, for open rooms, we should sometims carve a door on multiple sides anyway. So in fact door carving should
  -- be happening here ... or even right at the end.
  -- TODO: Finally finally, just because a wall is eligible doesn't mean it has connectivity. We need to use post placement
  -- analysis to check for open squares. In the mean time it might be sensible to only make the middle few squares of a wall
  -- eligible (and this will avoid silly corner attachments).
  local new_depth = 2
  if usage.depth ~= nil then new_depth = usage.depth + 1 end
  for n = 0, room_width - 1, 1 do
    -- Door wall
    local p1 = vaults_vector_add(room_base,{x = n, y = -1}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p1.x,p1.y,{ usage = "restricted", room = room, reason = "door" })
    -- Rear wall
    local p2 = vaults_vector_add(room_base,{x = n, y = room_height}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = wall_usage, normal = vector_rotate(normals[1],-v_normal_dir), room = room, depth = new_depth })
  end
  for n = 0, room_height - 1, 1 do
    -- Left wall
    local p3 = vaults_vector_add(room_base,{x = -1, y = n}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = wall_usage, normal = vector_rotate(normals[2],-v_normal_dir), room = room, depth = new_depth })
    -- Right wall
    local p4 = vaults_vector_add(room_base,{x = room_width, y = n}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = wall_usage, normal = vector_rotate(normals[4],-v_normal_dir), room = room, depth = new_depth })
  end

  -- TODO: We might want to get some data from the placed map e.g. tags and return it
  return true
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