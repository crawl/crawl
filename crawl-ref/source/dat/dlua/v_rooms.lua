------------------------------------------------------------------------------
-- v_rooms.lua:
--
-- This file handles the main task of selecting rooms and drawing them onto
-- the layout.
------------------------------------------------------------------------------

-- The four directions and their associated vector normal and name.
-- This helps us manage orientation of rooms.
local normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

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
--    if rotated.x == -0 then rotated.x = 0 end
  --  if rotated.y == -0 then rotated.y = 0 end
    count = count - 1
    if count <= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  if count < 0 then
    local rotated = { x = vec.y, y = -vec.x }
 --   if rotated.x == -0 then rotated.x = 0 end
  --  if rotated.y == -0 then rotated.y = 0 end
    count = count + 1
    if count >= 0 then return rotated end
    return vector_rotate(rotated,count)
  end
  return vec
end

local function pick_room(e, options)

  local weights = { }

  -- Do we just want an empty room? (Otherwise known as "floor vault" thanks to elliott)
  if crawl.random2(100) < options.empty_chance then
    local veto = true

    while veto do
      veto = false

      room = {
        type = "empty",
        size = { x = crawl.random_range(options.min_room_size,options.max_room_size), y = crawl.random_range(options.min_room_size,options.max_room_size) }
      }

      -- Describe connectivity of each wall. Of course all points are connectable since the room is empty.
      room.walls = { }
      local length
      for n = 0, 3, 1 do
        room.walls[n] = { eligible = true }
        if (n % 2) == 1 then length = room.size.y else length = room.size.x end
        room.walls[n].cells = {}
        for m = 1, length, 1 do
          room.walls[n].cells[m] = true
        end
      end

      -- Custom veto function can throw out the room now
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
      local mapdef = dgn.map_by_tag("vaults_"..room_type,true)
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

          -- Check all four directions for orient tag before we create the wals data, since the existence of a
          -- single orient tag makes the other walls ineligible
          local tags = {}
          local has_orient = false
          for n = 0, 3, 1 do
            if dgn.has_tag(room.map,"vaults_orient_" .. normals[n+1].name) then
              has_orient = true
              tags[n] = true
            end
          end

          -- Describe connectivity of each wall.
          room.walls = { }
          local length
          for n = 0, 3, 1 do
            local eligible = true
            if has_orient and not tags[n] then eligible = false end
            room.walls[n] = { eligible = eligible }
            -- Only need to compute wall data for an eligible wall
            if eligible then
              if (n % 2) == 1 then length = room.size.y else length = room.size.x end
              room.walls[n].cells = {}
              for m = 1, length, 1 do
                -- For now we'll assume the middle 3 or 4 squares have connectivity, this will solve 99% of cases with current
                -- subvaults (but make a somewhat boring layout). Really we should do a more detailed analysis of the map glyphs.
                if math.abs(m - 0 - length/2) <= 2 then room.walls[n].cells[m] = true else room.walls[n].cells[m] = false end
              end
            end
          end

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

  local perform_subst = true
  if dgn.has_tag(room.map, "preserve_wall") or room.wall_type == nil then perform_subst = false end
  result.stairs = { }
  for p in iter.rect_iterator(dgn.point(room.origin.x, room.origin.y), dgn.point(room.opposite.x, room.opposite.y)) do
    if (feat.is_stone_stair(p.x,p.y)) or
      -- On V:1 the branch entrant stairs don't count as stone_stair; we need to check specifically for the V exit stairs
      -- to avoid overwriting e.g. Crypt stairs!
      dgn.feature_name(dgn.grid(p.x,p.y)) == "return_from_vaults" then
      -- Remove the stair and remember it
      dgn.grid(p.x,p.y,"floor") --TODO: Be more intelligent about how to replace it
      table.insert(result.stairs, { pos = { x = p.x, y = p.y } })
    end
    -- Substitute rock walls for surrounding wall type
    if dgn.feature_name(dgn.grid(p.x,p.y)) == "rock_wall" then
      dgn.grid(p.x,p.y,room.wall_type)
    end
  end

end

function place_vaults_rooms(e,data, room_count, options)

  if options == nil then options = vaults_default_options() end
  local results = { }

  -- Attempt to place as many rooms as we've been asked for
  -- for i = 1, 10, 1 do
  for i = 1, room_count, 1 do
    local placed = false
    local tries = 0
    local maxTries = 50

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

  -- Now we need some stairs
  local stairs = { }
  for i, r in ipairs(results) do
    for j, s in ipairs(r.stairs) do
      if s ~= nil then table.insert(stairs,s) end
    end
  end

  local stair_types = {
    "stone_stairs_down_i", "stone_stairs_down_ii",
    "stone_stairs_down_iii", "stone_stairs_up_i",
    "stone_stairs_up_ii", "stone_stairs_up_iii" } -- , "escape_hatch_up" }"escape_hatch_down",

  -- Place three up, three down
  -- TODO: Optionally place some hatches / mimics too

  for n = 1, 6, 1 do
    -- Do we have any left? TODO: Could place some in random floor rooms, but hopefully the dungeon layout will provide missing stairs
    if #stairs == 0 then break end
    -- Any random stair
    local i = crawl.random_range(1, #stairs)
    local stair = stairs[i]
    -- Place it
    dgn.grid(stair.pos.x, stair.pos.y, stair_types[n])
    -- Remove from list
    table.remove(stairs,i)
  end


  -- Set MMT_VAULT across the whole map depending on usage. This way the dungeon builder knows where to place standard vaults without messing up the layout.
  local gxm, gym = dgn.max_bounds()
  for p in iter.rect_iterator(dgn.point(0, 0), dgn.point(gxm - 1, gym - 1)) do
    local usage = vaults_get_usage(data,p.x,p.y)
    if usage.usage == "restricted" or usage.usage == "eligible_open" or (usage.usage == "eligible" and usage.depth > 1) then
      dgn.set_map_mask(p.x,p.y)
    end
  end

  -- Useful to see what's going on when things are getting veto'd:
  dump_usage_grid_pretty(data)

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
    -- Brute force, this needs optimising, but for now try polling the grid up to 5000 times until we find an eligible spot
    while spotTries < maxSpotTries and not foundSpot do
      spotTries = spotTries + 1
      local spotx = crawl.random_range(2,gxm-2)
      local spoty = crawl.random_range(2,gym-2)
      spot = { x = spotx, y = spoty }
      usage = vaults_get_usage(usage_grid,spot.x,spot.y)
      if (usage.usage == "eligible_open" or usage.usage == "eligible" or usage.usage == "open") then
        foundSpot = true
      end
    end

    if foundSpot == true then
      -- Scan a 5x5 grid around an open spot, see if we find an eligible_open wall to attacn to. This makes us much more likely to
      -- join up rooms in open areas, and reduces the amount of 1-tile-width chokepoints between rooms. It's still fairly simplistic
      -- though and maybe we could do this around the whole room area later on instead?
      if usage.usage == "open" then
        local near_eligibles = {}
        for p in iter.rect_iterator(dgn.point(spot.x-2,spot.y-2),dgn.point(spot.x+2,spot.y+2)) do
          local near_usage = vaults_get_usage(usage_grid,p.x,p.y)
          if near_usage.usage == "eligible_open" then
            table.insert(near_eligibles, { spot = p, usage = near_usage })
          end
        end
        -- Randomly pick one of the new spots; maybe_place_vault will at least attempt to attach the room here instead, if possible
        if #near_eligibles > 0 then
          local picked = near_eligibles[crawl.random2(#near_eligibles)+1]
          spot = picked.spot
          usage = picked.usage
        end
      end

      if (usage.usage == "eligible" or usage.usage == "eligible_open") and usage.wall ~= nil then
        -- Link directly to this wall's door position
        -- TODO: Something slightly more interesting based on analysis of the room
        if usage.wall.door_position ~= nil then
          -- Change target spot to use this position
          spot = usage.wall.door_position
          vaults_get_usage(usage_grid,spot.x,spot.y)
        end
      end

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
  -- Choose a room orientation from  available ones (as determined in pick_room)
  local orients = { }
  for n = 0, 3, 1 do
    if room.walls[n].eligible then
      table.insert(orients,{ dir = n, wall = room.walls[n] })  -- TODO: We mainly need the dir right now; but should we save the wall a well for convenience?
    end
  end
  local count = #orients
  if count > 0 then
    local chosen = crawl.random2(count)
    orient = normals[orients[chosen + 1].dir + 1]  -- Base 1 arrays making things very confusing!
  else
    -- Should never happen, if there are no orients then all are enabled ...
    -- Pick one of the four orients now; if the room is rectangular we'll need
    -- to swap the width/height anyway
    orient = normals[crawl.random_range(1,#normals)]
  end
  -- Exchange width and height
  if orient.dir % 2 == 1 then room_width, room_height = room_height, room_width end

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

  local is_clear = true

  -- Check every square this room will go, including its walls, make sure we have space
  for p in iter.rect_iterator(dgn.point(-1, -1), dgn.point(room_width, room_height)) do

    local rx,ry = p.x,p.y

    -- Figure out where this target actually lies in map coords after rotation
    local target = vaults_vector_add(room_base, { x = rx, y = ry }, v_wall, v_normal )

    -- Check existing usage of that square
    local target_usage = vaults_get_usage(usage_grid,target.x,target.y)

    -- Check we are allowed to build here
    -- If any square is restricted then we fail. If our room type is "open" then we need to find all open or eligible_open squares (meaning we take up
    -- open space or we are attaching to an existing building). If our room type is "eligible" then we need to find all unused or eligible squares (meaning
    -- we are building into empty walls or attaching to more buildings).
    if (target_usage.usage == "restricted" or (usage.usage == "open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")
      or (usage.usage == "eligible" and target_usage.usage ~= "eligible" and target_usage.usage ~= "none")
      or (usage.usage == "eligible_open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")) then
      is_clear = false
      break
    end
  end

  -- For open rooms, check a border all around the outside of the wall. We don't want to land right next to another room because we could cut off its door,
  -- and also it would just create ugly two-thick walls.
  if (usage.usage=="open" or usage.usage == "eligible_open") then
    for p in iter.border_iterator(dgn.point(-2, -2), dgn.point(room_width + 1, room_height + 1)) do

      local target = vaults_vector_add(room_base, { x = p.x, y = p.y }, v_wall, v_normal )
      local target_usage = vaults_get_usage(usage_grid,target.x,target.y)

      -- Here we don't mind if restricted squares are outside the border, unless it's part of another vault's door.
      -- We don't want to butt up right next to an eligible wall without joining to it (or do we ...? it would mean less chokepoints existing... but could block parts of the scenery)
      if (target_usage.usage == "eligible" or (target_usage.usage == "eligible_open" and target_usage.room ~= usage.room)
         or (target_usage.usage == "restricted" and target_usage.reason ~= "vault" and target_usage.room ~= usage.room ) ) then
        is_clear = false
        break
      end
    end
  end

  -- No clear space found, the function fails and we'll look for a new spot
  if not is_clear then return false end

  -- Lookup the two corners of the room and map to oriented coords to find the top-leftmost and bottom-rightmost coords relative to dungeon orientation
  -- In particular we need the origin to eventually place the vault, and to fill in the floor/walls on the grid
  local c1,c2 = vaults_vector_add(room_base, { x = 0, y = 0 }, v_wall, v_normal ),vaults_vector_add(room_base, { x = room_width-1, y = room_height-1 }, v_wall, v_normal )
  local origin, opposite = { x = math.min(c1.x,c2.x), y = math.min(c1.y,c2.y) },{ x = math.max(c1.x,c2.x), y = math.max(c1.y,c2.y) }

  -- Store; will also be used for vault analysis
  room.origin = origin
  room.opposite = opposite

  -- Randomly vary the wall type
  -- TODO: Use spread from config instead and take note of level-wide wall type
  local surrounding_wall_type = "stone_wall"
  if crawl.one_chance_in(20) then surrounding_wall_type = "rock_wall" end
  if crawl.one_chance_in(15) then surrounding_wall_type = "metal_wall" end
  if crawl.one_chance_in(25) then surrounding_wall_type = "green_crystal_wall" end
  -- Store it so we can perform substitution after placement
  room.wall_type = surrounding_wall_type

  -- Fill the wall and then floor space inside
  dgn.fill_grd_area(origin.x - 1, origin.y - 1, opposite.x + 1, opposite.y + 1, surrounding_wall_type)
  dgn.fill_grd_area(origin.x, origin.y, opposite.x, opposite.y, "floor")

  -- Actually place a map inside the room if we have one
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

  -- How big the door?
  local needs_door = true  -- Only reason we wouldn't need a door is if we're intentionally creating a dead end, not implemented yet
  if needs_door then
    -- Even or odd width
    local oddness = room_width % 2
    -- Work out half the door length
--    local door_max = math.floor(room_width / 2)
--    local door_half_length = crawl.random_range(1,door_max)
    -- Multiply by two and add one if odd
--    local door_length = door_half_length * 2 + oddness
    local door_length = 4
    if oddness == 1 then door_length = 3 end

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

      dgn.fill_grd_area(window_1l.x, window_1l.y, window_2l.x, window_2l.y, "clear_stone_wall") -- TODO: Vary window type
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
      -- Could restrict next to walls if we want to e.g. create large empty areas and build new cities inside them
      -- if x == origin.x or x == opposite.x or y == origin.y or y == opposite.y then
        -- vaults_set_usage(usage_grid,x,y,{usage = "restricted"})
      -- else

      -- Setting usage as "empty" means the empty space will be available for vault placement by the dungeon builder
      vaults_set_usage(usage_grid,x,y,{usage = "empty"})
      -- end
    end
  end

  -- Set wall usage. For each wall of the room set it to eligible or restricted depending on if it's the entrant wall or one of
  -- the others, and on the vaults_orient_* tags.
  local wall_usage, new_depth = "eligible", 2
  if usage.usage == "open" or usage.usage == "eligible_open" then wall_usage = "eligible_open" end
  if usage.depth ~= nil then new_depth = usage.depth + 1 end  -- Room depth

  -- TODO: if a wall overlaps with another eligible wall (that supports an exit) then we should carve a new door.
  -- TODO: Furthermore, for open rooms, we should sometims carve a door on multiple sides anyway. So in fact door carving should
  -- be happening here ... or even right at the end.

  -- Perform analysis on each wall (this stuff could be moved to post_placement_analysis)
  -- TODO: Draw some diagrams and figure this out properly
  -- for w = 0, 3, 1 do
  --  local wall = room.walls[w]
  --  local rotated_dir = (w + v_normal_dir) % 4
  --  local wall_normal = normals[rotated_dir + 1]
  --  local wall_dir =
  -- end

  -- We don't do anything with the corners, only adjacent walls, because it's fine for other rooms to overlap corners
  local rear_wall = room.walls[(1 - v_normal_dir) % 4]
  for n = 0, room_width - 1, 1 do
    -- Door wall
    local p1 = vaults_vector_add(room_base,{x = n, y = -1}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p1.x,p1.y,{ usage = "restricted", room = room, reason = "door" })
    -- Rear wall
    local p2 = vaults_vector_add(room_base,{x = n, y = room_height}, v_wall, v_normal)
    -- Look up the right wall number, adjusting for room orientation. Check if wall allows door then either restrict or make eligible...
    if rear_wall.eligible then
      vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = wall_usage, normal = vector_rotate(normals[1],-v_normal_dir), room = room, depth = new_depth, wall = rear_wall })
      if room.type == "vault" and n == math.floor(room_width / 2) then
        rear_wall.door_position = p2
      end
    else
      -- TODO: Restricted is slightly wrong. Another room should be allowed to share a partitioning wall even if there's no door.
      vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = "restricted", room = room, reason = "orient" })
    end
  end
  local left_wall = room.walls[(2 - v_normal_dir) % 4]
  local right_wall = room.walls[(4 - v_normal_dir) % 4]
  for n = 0, room_height - 1, 1 do
    -- Left wall
    local p3 = vaults_vector_add(room_base,{x = -1, y = n}, v_wall, v_normal)
    local wall = room.walls[(1 - v_normal_dir)%4]
    if wall.eligible then
      vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = wall_usage, normal = vector_rotate(normals[2],-v_normal_dir), room = room, depth = new_depth, wall = left_wall })
      if room.type == "vault" and n == math.floor(room_height / 2) then
        left_wall.door_position = p3
      end
    else
      vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = "restricted", room = room, reason = "orient" })
    end

    -- Right wall
    local p4 = vaults_vector_add(room_base,{x = room_width, y = n}, v_wall, v_normal)

    if wall.eligible then
      vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = wall_usage, normal = vector_rotate(normals[4],-v_normal_dir), room = room, depth = new_depth, wall = right_wall })
      if room.type == "vault" and n == math.floor(room_height / 2) then
        right_wall.door_position = p4
      end
    else
      vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = "restricted", room = room, reason = "orient" })
    end

  end

  -- TODO: We might want to get some data from the placed map e.g. tags and return it
  return true
end
