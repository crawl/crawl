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

  -- Pick generator from weighted table
  local chosen = util.random_weighted_from("weight",options.room_type_weights)

  -- TODO: Proceduralise the generators more so the weights table can contain functions that generate the room contents, answer questions about
  -- size, connectable walls, etc.

  -- Floor vault (empty rooms)
  if chosen.generator == "floor" then
    local veto = true
    local floor_feat = dgn.feature_number("floor")
    local diff = chosen.max_size - chosen.min_size + 1

    while veto do
      veto = false
      room = {
        type = "empty",
        -- Use random2(random2(..)) to get a room size that tends towards lower values
        size = { x = chosen.min_size + crawl.random2(crawl.random2(diff)), y = chosen.min_size + crawl.random2(crawl.random2(diff)) }
      }
      -- Describe connectivity of each wall. Of course all points are connectable since the room is empty.
      room.walls = { }
      local length
      for n = 0, 3, 1 do
        room.walls[n] = { eligible = true }
        if (n % 2) == 1 then length = room.size.y else length = room.size.x end
        room.walls[n].cells = {}
        for m = 1, length, 1 do
          room.walls[n].cells[m] = { feat = floor_feat, open = true }
        end
      end

      -- Custom veto function can throw out the room now
      if options.veto_room_callback ~= nil then
        local result = options.veto_room_callback(room)
        if result == true then veto = true end
      end

    end

    return room

  -- Pick by tag
  elseif chosen.generator == "tagged" then
    local found = false
    local tries = 0
    local maxTries = 20

    while tries < maxTries and not found do
      -- Resolve a map with the specified tag
      local mapdef = dgn.map_by_tag(chosen.tag,true)
      -- Temporarily prevent map getting mirrored / rotated during resolution; and hardwire transparency because lack of it can fail a whole layout
      dgn.tags(mapdef, "no_vmirror no_hmirror no_rotate transparent");
      -- Resolve the map so we can find its width / height
      local map, vplace = dgn.resolve_map(mapdef,true,true)
      local room_width,room_height

      if map == nil then
        -- If we can't find a map then we're probably not going to find one
        return nil
      else
        -- Allow map to be flipped and rotated again, otherwise we'll struggle later when we want to rotate it into the correct orientation
        dgn.tags_remove(map, "no_vmirror no_hmirror no_rotate")

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
              local wall_normal = normals[n+1]
              local wall_dir = vector_rotate(wall_normal,1)
              -- Get the coord of the first cell of the wall
              local start = { x = 0, y = 0 }
              if n == 1 then start = { x = 0, y = room.size.y - 1 }
              elseif n == 2 then start = { x = room.size.x - 1, y = room.size.y - 1 }
              elseif n == 3 then start = { x = room.size.x - 1, y = 0 } end

              if (n % 2) == 1 then length = room.size.y else length = room.size.x end
              room.walls[n].cells = {}
              -- For every cell of the wall, check the internal feature of the room
              for m = 1, length, 1 do
                -- Map to internal room coordinate based on start vector and normals
                local mapped = vaults_vector_add(start, { x = m - 1, y = 0 }, wall_dir,wall_normal)
                local feature = dgn.inspect_map(vplace,mapped.x,mapped.y)
                local solid = feat.is_solid(feature)
                local water = feat.is_water(feature)
                room.walls[n].cells[m] = { feature = feature, open = not (solid or water) }
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
  -- dump_usage_grid(data)

  return true
end

function place_vaults_room(e,usage_grid,room, options)

  local gxm, gym = dgn.max_bounds()

  local maxTries = 50
  local tries = 0
  local done = false

  local available_spots = #(usage_grid.eligibles)
  if available_spots == 0 then return { placed = false } end

  -- Have a few goes at placing; we'll select a random eligible spot each time and try to place the vault there
  while tries < maxTries and not done do
    tries = tries + 1

    -- Select an eligible spot from the list in usage_grid
    local usage = usage_grid.eligibles[crawl.random_range(1,available_spots)]
    local spot = usage.spot
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

    -- Attempt to place the room on the wall at a random position
    if vaults_maybe_place_vault(e, spot, usage_grid, usage, room, options) then
      done = true
    end
  end
  return { placed = done }
end

-- Attempts to place a room by picking a random available spot
function vaults_maybe_place_vault(e, pos, usage_grid, usage, room, options)
  local v_normal, v_wall, clear_bounds, orient, v_normal_dir,wall

  -- Choose which wall to attach available ones (as determined in pick_room)
  local orients = { }
  for n = 0, 3, 1 do
    if room.walls[n].eligible then
      table.insert(orients,{ dir = n, wall = room.walls[n] })  -- TODO: We mainly need the dir right now; but should we save the wall a well for convenience?
    end
  end
  local count = #orients
  if count == 0 then return false end

  local chosen = orients[crawl.random2(count) + 1]
  wall = chosen.wall
  orient = normals[chosen.dir + 1]

  -- Pick a random open cell on the wall. This is where we'll attach the room to the door.
  local open_cells = {}
  for i, cell in ipairs(wall.cells) do
    if cell.open then table.insert(open_cells, { pos = i, cell = cell }) end
  end
  if #open_cells == 0 then return false end -- Shouldn't happen, eligible should mean cells available
  local chosen_cell = open_cells[crawl.random2(#open_cells)+1]

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  -- In open space we can pick a random normal for the door
  if usage.usage == "open" then
    -- Pick a random rotation for the door wall
    -- TODO: We might as well try all four?
    v_normal = normals[crawl.random_range(1,4)]
    v_normal_dir = v_normal.dir
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
  end

  -- If placing a room in a wall we have the normal in usage
  if usage.usage == "eligible" or usage.usage == "eligible_open" then
    v_normal = usage.normal -- TODO: make sure usage.normal.dir is never nil and remove this loop
    for i,n in ipairs(normals) do
      if n.x == v_normal.x and n.y == v_normal.y then
        v_normal_dir = n.dir
      end
    end
    v_wall = vector_rotate(v_normal, 1)  -- Wall normal is perpendicular
  end

  -- Check every square this room will go, including its walls, to make sure we have space
  local is_clear, room_width, room_height = true, room.size.x, room.size.y
  -- Exchange width and height if the room has rotated
  if orient.dir % 2 == 1 then room_width, room_height = room_height, room_width end

  -- Now we figure out the actual grid coord where the bottom left corner of the room will go
  local room_base = vaults_vector_add(pos, { x = chosen_cell.pos - room_width, y = 1 }, v_wall, v_normal)

  -- Can veto room from options
  if options.veto_place_callback ~= nil then
    local result = options.veto_place_callback(usage,room)
    if result == true then return false end
  end

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

  -- Wall type. Originally this was randomly varied but we decided it didn't work very well especially when rooms were connected.
  local surrounding_wall_type = options.layout_wall_type
--  if crawl.one_chance_in(20) then surrounding_wall_type = "rock_wall" end
--  if crawl.one_chance_in(15) then surrounding_wall_type = "metal_wall" end
--  if crawl.one_chance_in(25) then surrounding_wall_type = "green_crystal_wall" end
  -- Store it so we can perform substitution after placement
  room.wall_type = surrounding_wall_type

  -- Fill the wall and then floor space inside
  dgn.fill_grd_area(origin.x - 1, origin.y - 1, opposite.x + 1, opposite.y + 1, surrounding_wall_type)
  dgn.fill_grd_area(origin.x, origin.y, opposite.x, opposite.y, "floor")

  -- Calculate the final orientation we need for the room to match the door
  -- Somewhat worked out by trial and error but it appears to work
  local final_orient = (v_normal_dir - orient.dir + 2) % 4

  -- Actually place a map inside the room if we have one
  if room.type == "vault" then

    -- We can only rotate a map clockwise or anticlockwise. To rotate 180 we just flip on both axes.
    if final_orient == 0 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,0,true,true)
    elseif final_orient == 1 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,-1,true,true)
    elseif final_orient == 2 then dgn.reuse_map(room.vplace,origin.x,origin.y,true,true,0,true,true)
    elseif final_orient == 3 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,1,true,true) end

  end

  -- How big the door?
  local needs_door = true  -- Only reason we wouldn't need a door is if we're intentionally creating a dead end, not implemented yet
  if needs_door then
    -- Even or odd room width
    local oddness = room_width % 2
    -- Doors should be 1 or 2 wide. Right now this depends on whether the wall has odd or even length. This isn't ideal, should be randomised
    -- more, but things generally look weird if they're off-center.
    local door_length = 2 - oddness
    -- Rarish chance of a slightly bigger door, TODO: Discuss more with elliptic & st
    -- For now will keep things highly predictable so the layouts work properly. But it really shouldn't be too hard at this stage
    -- to check for spots where both sides of the wall have floor, and carve the door there. (Would be nice IMO to extremely rarely have
    -- the very big doors, perhaps only on the empty rooms).
    -- if crawl.one_chance_in(6) then door_length = door_length + 2

    -- Work out half the door length
--    local door_max = math.floor(room_width / 2)
--    local door_half_length = crawl.random_range(1,door_max)
    -- Multiply by two and add one if odd
--    local door_length = door_half_length * 2 + oddness

    local door_start = (room_width - door_length) / 2
    local door_c1 = vaults_vector_add(room_base, { x = door_start, y = -1 }, v_wall, v_normal)
    local door_c2 = vaults_vector_add(room_base, { x = room_width - 1 - door_start, y = -1 }, v_wall, v_normal)

    -- dgn.fill_grd_area(door_c1.x, door_c1.y, door_c2.x, door_c2.y, "closed_door")
    door_length = 1
    dgn.grid(pos.x,pos.y, "closed_door")  -- Placing door only on original spot for now, figure out bigger doors later

    -- Optionally generate windows (temp. disabled)
    if false and door_length < room_width and crawl.one_chance_in(5) and (room.map == nil or not dgn.has_tag(room.map,"vaults_no_windows")) then
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
  local set_empty = crawl.coinflip()
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

      -- Setting usage as "empty" means the empty space won't be filled but will be available for vault placement by the dungeon builder
      if set_empty then
        vaults_set_usage(usage_grid,x,y,{usage = "empty"})
      elseif x > origin.x + 1 and x < opposite.x-1 and y > origin.y - 1 and y < opposite.y - 1 then  -- Leaving a 2 tile border
        vaults_set_usage(usage_grid,x,y,{usage = "open"})
      else
        vaults_set_usage(usage_grid,x,y,{usage = "restricted", reason = "border"})
      end
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
  local wall_orient = orient.dir
  local rear_wall = room.walls[(wall_orient + 2) % 4]
  for n = 0, room_width - 1, 1 do
    -- Door wall
    local p1 = vaults_vector_add(room_base,{x = n, y = -1}, v_wall, v_normal)
    vaults_set_usage(usage_grid,p1.x,p1.y,{ usage = "restricted", room = room, reason = "door" })
    -- Rear wall
    local p2 = vaults_vector_add(room_base,{x = n, y = room_height}, v_wall, v_normal)
    -- Look up the right wall number, adjusting for room orientation. Check if wall allows door then either restrict or make eligible...
    if rear_wall.eligible then
      local rear_cell = rear_wall.cells[n + 1]
      if rear_cell ~= nil and rear_cell.open then
        vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = wall_usage, normal = vector_rotate(normals[1],-v_normal_dir), room = room, depth = new_depth, wall = rear_wall, cell = rear_cell })
      else
        vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = "restricted", room = room, reason = "cell" })
      end
    else
      -- TODO: Restricted is slightly wrong. Another room should be allowed to share a partitioning wall even if there's no door.
      vaults_set_usage(usage_grid,p2.x,p2.y,{ usage = "restricted", room = room, reason = "orient" })
    end
  end
  local left_wall = room.walls[(wall_orient + 3) % 4]
  local right_wall = room.walls[(wall_orient + 1) % 4]
  for n = 0, room_height - 1, 1 do
    -- Left wall
    local p3 = vaults_vector_add(room_base,{x = -1, y = n}, v_wall, v_normal)
    if left_wall.eligible then
      local left_cell = left_wall.cells[n + 1]
      if left_cell ~= nil and left_cell.open then
        vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = wall_usage, normal = vector_rotate(normals[2],-v_normal_dir), room = room, depth = new_depth, wall = left_wall, cell = left_cell })
      else
        vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = "restricted", room = room, reason = "cell" })
      end
    else
      vaults_set_usage(usage_grid,p3.x,p3.y,{ usage = "restricted", room = room, reason = "orient" })
    end

    -- Right wall
    local p4 = vaults_vector_add(room_base,{x = room_width, y = n}, v_wall, v_normal)

    if right_wall.eligible then
      local right_cell = right_wall.cells[room_height - n]
      if right_cell ~= nil and right_cell.open then
        vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = wall_usage, normal = vector_rotate(normals[4],-v_normal_dir), room = room, depth = new_depth, wall = right_wall, cell = right_cell })
      else
        vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = "restricted", room = room, reason = "cell" })
      end
    else
      vaults_set_usage(usage_grid,p4.x,p4.y,{ usage = "restricted", room = room, reason = "orient" })
    end

  end

  -- TODO: We might want to get some data from the placed map e.g. tags and return it
  return true
end
