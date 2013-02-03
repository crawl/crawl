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
-- Highly rudimentary, and not actually used for anything, keeping around for now just in case.
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

local function make_code_room(options,chosen)
  -- Pick a size for the room
  local size
  if chosen.generator.pick_size_callback ~= nil then
    size = chosen.generator.pick_size_callback(options)
  else
    local min_size, max_size = options.min_room_size,options.max_room_size
    if chosen.generator.min_size ~= nil then min_size = chosen.generator.min_size end
    if chosen.generator.max_size ~= nil then max_size = chosen.generator.max_size end
    local diff = max_size - min_size + 1
    size = { x = min_size + crawl.random2(crawl.random2(diff)), y = min_size + crawl.random2(crawl.random2(diff)) }
  end

  room = {
    type = "grid",
    size = size,
    generator_used = chosen,
    grid = vaults_new_layout(size.x,size.y)
  }

  -- Make grid empty space
  for n = 0, size.x - 1, 1 do
    for m = 0, size.y - 1, 1 do
      vaults_set_layout(room.grid,n,m, { solid = true, space = true, feature = "space" })
    end
  end

  -- Paint and decorate the layout grid
  paint_grid(chosen.paint_callback(room,options),options,room.grid)
  if _VAULTS_DEBUG then dump_layout_grid(room.grid) end
  if chosen.decorate_callback ~= nil then chosen.decorate_callback(room.grid,room,options) end  -- Post-production

  return room
end

-- Pick a map by tag and make a room grid from it
local function make_tagged_room(options,chosen)
  -- Resolve a map with the specified tag
  local mapdef = dgn.map_by_tag(chosen.tag,true)
  if mapdef == nil then return nil end  -- Shouldn't happen when thing are working but just in case

  -- Temporarily prevent map getting mirrored / rotated during resolution; and hardwire transparency because lack of it can fail a whole layout
  dgn.tags(mapdef, "no_vmirror no_hmirror no_rotate transparent");
  -- Resolve the map so we can find its width / height
  local map, vplace = dgn.resolve_map(mapdef,false)
  local room,room_width,room_height

    -- If we can't find a map then we're probably not going to find one
  if map == nil then return nil end
  -- Allow map to be flipped and rotated again, otherwise we'll struggle later when we want to rotate it into the correct orientation
  dgn.tags_remove(map, "no_vmirror no_hmirror no_rotate")

  local room_width,room_height = dgn.mapsize(map)
  local veto = false
  if not (room_width > options.max_room_size or room_width < options.min_room_size or room_height > options.max_room_size or room_height < options.min_room_size) then
    room = {
      type = "vault",
      size = { x = room_width, y = room_height },
      map = map,
      vplace = vplace,
      generator_used = chosen,
      grid = {}
    }

    room.preserve_wall = dgn.has_tag(room.map, "preserve_wall")
    room.no_windows = dgn.has_tag(room.map, "no_windows")

    -- Check all four directions for orient tag before we create the wals data, since the existence of a
    -- single orient tag makes the other walls ineligible
    room.tags = {}
    for n = 0, 3, 1 do
      if dgn.has_tag(room.map,"vaults_orient_" .. normals[n+1].name) then
        room.has_orient = true
        room.tags[n] = true
      end
    end
    -- Make grid by feature inspection; will be used to compute wall data
    for m = 0, room.size.y - 1, 1 do
      room.grid[m] = {}
      for n = 0, room.size.x - 1, 1 do
        local inspected = { }
        inspected.feature, inspected.exit, inspected.space = dgn.inspect_map(vplace,n,m)
        inspected.solid = not feat.has_solid_floor(inspected.feature)
        inspected.feature = dgn.feature_name(inspected.feature)
        room.grid[m][n] = inspected
      end
    end
    found = true
  end
  return room
end

local function pick_room(e, options)

  -- Filters out generators that have reached their max
  local function weight_callback(generator)
    if generator.max_rooms ~= nil and generator.placed_count ~= nil and generator.placed_count >= generator.max_rooms then
      return 0
    end
    return generator.weight
  end
  -- Pick generator from weighted table
  local chosen = util.random_weighted_from(weight_callback,options.room_type_weights)
  local room

  -- Main generator loop
  local veto,tries,maxTries = false,0,50
  while tries < maxTries and (room == nil or veto) do
    tries = tries + 1
    veto = false

    -- Code rooms
    if chosen.generator == "code" then
      room = make_code_room(options, chosen)

    -- Pick vault map by tag
    elseif chosen.generator == "tagged" then
      room = make_tagged_room(options,chosen)
    end

    if room ~= nil and room.grid ~= nil then

      -- Create a grid for the wall mask. It's 1 unit bigger on all sides than the standard grid. In this we will track where we need to place walls and also
      -- if those walls are connected, i.e. border onto a passable square in such a way that they can be used as a door.
      room.walls = { }
      room.mask_grid = { }
      local has_exits = false
      for m = -1, room.size.y, 1 do
        room.mask_grid[m] = {}
        for n = -1, room.size.x, 1 do
          local cell = vaults_get_layout(room.grid,n,m)

          if cell.exit then has_exits = true end

          if not cell.space then
            room.mask_grid[m][n] = { vault = true, wall = false, space = false }
          else
            -- Analyse layout squares around it
            local all_space = true
            for ax = n-1, n+1, 1 do
              for ay = m-1, m+1, 1 do
                if ax ~= n and ay ~= m then
                  local near_cell = vaults_get_layout(room.grid,ax,ay)
                  if not near_cell.space then all_space = false end
                end
              end
            end
            if all_space then
              room.mask_grid[m][n] = { space = true, wall = false, vault = false }
            else
              local adj_cells = {
                vaults_get_layout(room.grid,n,m-1),
                vaults_get_layout(room.grid,n-1,m),
                vaults_get_layout(room.grid,n,m+1),
                vaults_get_layout(room.grid,n+1,m)
              }
              local adj_space,adj_exit,adj_exit_cell = 0,0,nil
              for i,adj_cell in ipairs(adj_cells) do
                if adj_cell.space then
                  adj_space = adj_space + 1
                else
                  if not adj_cell.solid then
                    adj_exit = i
                    adj_exit_cell = adj_cell
                  end
                end
              end
              local wall_mask
              if adj_space == 3 and adj_exit > 0 then
                wall_mask = { wall = true, connected = true, dir = (adj_exit + 1) % 4, cell = cell, exit_cell = adj_exit_cell }
              else
                wall_mask = { wall = true }
              end
              room.mask_grid[m][n] = wall_mask
            end
          end
        end
      end

      -- Loop through the cells again and this time determine openness. Has to be a separate loop because we needed to already know has_exits.
      for m = -1, room.size.y, 1 do
        for n = -1, room.size.x, 1 do
          local wall_mask = room.mask_grid[m][n]
          if wall_mask.connected then
            local cell = wall_mask.exit_cell
            local is_open = true
            if cell.space or (has_exits and not cell.exit) then
              is_open = false
            end
            wall_mask.cell.open = is_open
            if not is_open then wall_mask.connected = false
            else
              -- Remember in a list of wall cells for connecting
              local wall_cell = { feature = cell.feature, exit = cell.exit, exit_cell = cell, cell = wall_mask.cell, open = is_open, space = cell.space, pos = { x = n, y = m }, dir = wall_mask.dir }
              if room.walls[wall_cell.dir] == nil then room.walls[wall_cell.dir] = {} end
              table.insert(room.walls[wall_cell.dir], wall_cell)
            end
          end
        end
      end
      for n = 0, 3, 1 do
        if room.walls[n] == nil then
          room.walls[n] = { eligible = false }
        else
          room.walls[n].eligible = true
        end
      end

      -- Allow veto callback to throw this room out (e.g. on size, exits)
      if options.veto_room_callback ~= nil then
        veto = options.veto_room_callback(room)
      end
    end
  end

  return room

end

local function analyse_vault_post_placement(usage_grid,room,result,options)
  local perform_subst = true
  if room.preserve_wall or room.wall_type == nil then perform_subst = false end
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

function place_vaults_rooms(e, data, room_count, options)

  if options == nil then options = vaults_default_options() end
  local results, rooms_placed, times_failed, total_failed = { }, 0, 0, 0

  -- Keep trying to place rooms until we've had max_room_tries fail in a row or we reach the max
  while rooms_placed < room_count and times_failed < options.max_room_tries do
    local placed = false

    -- Pick a room
    local room = pick_room(e, options)
    -- Attempt to place it. The placement function will try several times to find somewhere to fit it.
    if room ~= nil then
      local result = place_vaults_room(e,data,room,options)
      if result ~= nil and result.placed then
        placed = true
        rooms_placed = rooms_placed + 1  -- Increment # rooms placed
        times_failed = 0 -- Reset fail count
        if room.type == "vault" then
          -- Perform analysis for stairs
          analyse_vault_post_placement(data,room,result,options)
          table.insert(results,result)
        end
        -- Increment the count of rooms of this type
        if room.generator_used ~= nil then
          if room.generator_used.placed_count == nil then room.generator_used.placed_count = 0 end
          room.generator_used.placed_count = room.generator_used.placed_count + 1
        end
      end

    end

    if not placed then
      times_failed = times_failed + 1 -- Increment failure count
      total_failed = total_failed + 1
    end

  end

  -- Now we need some stairs
  local stairs = { }
  for i, r in ipairs(results) do
    for j, s in ipairs(r.stairs) do
      if s ~= nil then table.insert(stairs,s) end
    end
  end

  -- Place three up, three down, plus two of each hatch
  local stair_types = {
    "stone_stairs_down_i", "stone_stairs_down_ii",
    "stone_stairs_down_iii", "stone_stairs_up_i",
    "stone_stairs_up_ii", "stone_stairs_up_iii",
    "escape_hatch_up", "escape_hatch_down",
    "escape_hatch_up", "escape_hatch_down" }

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
    if usage ~= nil and usage.usage == "restricted" then -- or usage.usage == "eligible_open" or (usage.usage == "eligible" and usage.depth > 1) then
      dgn.set_map_mask(p.x,p.y)
    end
  end

  -- Useful to see what's going on when things are getting veto'd:
  if _VAULTS_DEBUG then dump_usage_grid(data) end

  return true
end

function place_vaults_room(e,usage_grid,room, options)

  local gxm, gym = dgn.max_bounds()

  local tries = 0
  local done = false

  local available_spots = #(usage_grid.eligibles)
  if available_spots == 0 then return { placed = false } end

  -- Have a few goes at placing; we'll select a random eligible spot each time and try to place the vault there
  while tries < options.max_place_tries and not done do
    tries = tries + 1
    -- Select an eligible spot from the list in usage_grid
    local usage = usage_grid.eligibles[crawl.random_range(1,available_spots)]
    local spot = usage.spot
    -- Scan a 5x5 grid around an open spot, see if we find an eligible_open wall to attach to. This makes us much more likely to
    -- join up rooms in open areas, and reduces the amount of 1-tile-width chokepoints between rooms. It's still fairly simplistic
    -- though and maybe we could do this around the whole room area later on instead?
    if usage.usage == "open" then
      local near_eligibles = {}
      for p in iter.rect_iterator(dgn.point(spot.x-2,spot.y-2),dgn.point(spot.x+2,spot.y+2)) do
        local near_usage = vaults_get_usage(usage_grid,p.x,p.y)
        if near_usage ~= nil and near_usage.usage == "eligible_open" then
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

local function decorate_walls(room, connections, door_required, has_windows)

  -- TODO: Use decorator callbacks for these so we can vary by layout
  local have_door = true
  if not door_required then
    have_door = crawl.x_chance_in_y(4,5)
  end

  if have_door then
  local num_doors = math.abs(crawl.random2avg(9,4)-4)+1  -- Should tend towards 1 but rarely can be up to 5
    for n=1,num_doors,1 do
      -- This could pick a door we've already picked; doesn't matter, this just makes more doors even slightly
      -- less likely
      local door = connections[crawl.random2(#connections)+1]
      if door ~= nil then
        dgn.grid(door.grid_pos.x,door.grid_pos.y, "closed_door")
        door.is_door = true
      end
    end
  end

  -- Optionally place windows
  if has_windows then
    local num_windows = math.abs(crawl.random2avg(5,3)-2)+2  -- Should tend towards 2 but rarely can be up to 4

    for n=1,num_windows,1 do
      -- This could pick a door we've already picked; doesn't matter, this just makes more doors even slightly less likelier
      local door = connections[crawl.random2(#connections)+1]
      if door ~= nil and not door.is_door then
        dgn.grid(door.grid_pos.x,door.grid_pos.y, "clear_stone_wall")
        door.is_window = true
      end
    end
  end
end

-- Attempts to place a room by picking a random available spot
function vaults_maybe_place_vault(e, pos, usage_grid, usage, room, options)
  local v_normal, v_wall, clear_bounds, orient, v_normal_dir,wall

  -- Choose which wall of the room to attach to this spot (walls were determined in pick_room)
  local orients = { }
  for n = 0, 3, 1 do
    if room.walls[n].eligible then
      table.insert(orients,{ dir = n, wall = room.walls[n] })
    end
  end
  local count = #orients
  if count == 0 then return false end

  local chosen = orients[crawl.random2(count) + 1]
  wall = chosen.wall
  orient = normals[chosen.dir + 1]

  -- Pick a random open cell on the wall. This is where we'll attach the room to the door.
  if #wall == 0 then return false end -- Shouldn't happen, eligible should mean cells available
  local chosen_wall = wall[crawl.random2(#wall)+1]
  local chosen_cell = wall.cell

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  -- In open space we can pick a random normal for the door
  if usage.usage == "open" then
    -- Pick a random rotation for the door wall
    v_normal = normals[crawl.random_range(1,4)]
    v_normal_dir = v_normal.dir
  end

  -- If placing a room in an existing wall we have the normal stored alredy in the usage data
  if usage.usage == "eligible" or usage.usage == "eligible_open" then
    v_normal = usage.normal -- TODO: make sure usage.normal.dir is never nil and remove this loop
    for i,n in ipairs(normals) do
      if n.x == v_normal.x and n.y == v_normal.y then
        v_normal_dir = n.dir
      end
    end
  end

  local is_clear, room_width, room_height = true, room.size.x, room.size.y

  -- Figure out the mapped x and y vectors of the room relative to its orient
  local room_final_y_dir = (v_normal_dir - chosen_wall.dir + 2) % 4
  local room_final_x_dir = (room_final_y_dir + 1) % 4
  local room_final_x_normal = normals[room_final_x_dir+1]
  local room_final_y_normal = normals[room_final_y_dir+1]

  -- Now we can use those vectors along with the position of the connecting wall within the room, to find out where the (0,0) corner
  -- of the room lies on the map (and use that coord for subsequent calculations within the room grid)
  local room_base = vaults_vector_add(pos, { x = -chosen_wall.pos.x, y = -chosen_wall.pos.y }, room_final_x_normal, room_final_y_normal)

  -- Layout configuration can now veto this room placement with a callback
  if options.veto_place_callback ~= nil then
    local result = options.veto_place_callback(usage,room,pos,chosen_wall,room_base) -- TODO: Package all these params into a state object
    if result == true then return false end
  end

  -- Loop through all coords of the room and its walls. Map to grid coords and add all this data into a list to make iterating through
  -- map squares easier in the future
  local coords_list = {}

  for m = -1, room.size.y, 1 do
    for n = -1, room.size.x, 1 do
      local coord = { room_pos = { x = n, y = m } }
      coord.grid_pos = vaults_vector_add(room_base, coord.room_pos, room_final_x_normal, room_final_y_normal)
      coord.grid_usage = vaults_get_usage(usage_grid, coord.grid_pos.x, coord.grid_pos.y)
      -- Out of bounds (we could simply catch this earlier)
      if coord.grid_usage == nil then
        is_clear = false
        break
      end
      coord.room_cell = vaults_get_layout(room.grid,coord.room_pos.x,coord.room_pos.y)
      coord.room_mask = vaults_get_layout(room.mask_grid,coord.room_pos.x,coord.room_pos.y)
      -- Check cells of non-space or wall
      if coord.room_mask.vault or coord.room_mask.wall then
        local target_usage = coord.grid_usage
        -- Check we are allowed to build here
        -- If our room type is "open" then we need to find all open or eligible_open squares (meaning we take up
        -- open space or we are attaching to an existing building). If our room type is "eligible" then we need to find all unused or eligible squares (meaning
        -- we are building into empty walls or attaching to more buildings). "Restricted" squares only need to fail for certain reasons; quite often our walls will
        -- overwrite walls of other rooms which may be "restricted" due to the contents of the room, but restricted squares inside vaults or too near to existing walls
        -- ("border") must fail.
        if (target_usage == nil or (target_usage.usage == "restricted" and (target_usage.reason == "border" or target_usage.reason == "vault" or target_usage.reason == "door"))
          or (usage.usage == "open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")
          or (usage.usage == "eligible" and target_usage.usage ~= "eligible" and target_usage.usage ~= "none")
          or (usage.usage == "eligible_open" and target_usage.usage ~= "eligible_open" and target_usage.usage ~= "open")) then
            is_clear = false
          break
        end

        table.insert(coords_list, coord)
      end
    end
    if not is_clear then break end
  end

  -- Calculate the two corner points including walls. Calculating this first rather than for each checking coord proved to be
  -- a significant optimisation; but this will mean to correctly do empty space collapsing we'd have to first create a correctly transformed
  -- map of the space which would negate this optimisation. However we only need to compute the space once for each of the four orientations
  -- so maybe that should happen in the pick_room phase.
  local space_tl,space_br = vaults_vector_add(room_base, { x = -1, y = -1 }, room_final_x_normal, room_final_y_normal),vaults_vector_add(room_base, { x = room.size.x, y = room.size.y }, room_final_x_normal, room_final_y_normal)
  local space_p1,space_p2 = dgn.point(math.min(space_tl.x,space_br.x),math.min(space_tl.y,space_br.y)),dgn.point(math.max(space_tl.x,space_br.x),math.max(space_tl.y,space_br.y))

  -- For open rooms, check a border all around the outside of the wall. We don't want to land right next to another room because we could cut off its door,
  -- and also it would just create ugly two-thick walls.
  -- TODO: Non-trivial to space collapse, shouldn't be too bad for now
  if (is_clear and usage.usage == "open" or usage.usage == "eligible_open") then
    for target in iter.border_iterator(dgn.point(space_p1.x-1, space_p1.y-1), dgn.point(space_p2.x + 1, space_p2.y + 1)) do

      local target_usage = vaults_get_usage(usage_grid,target.x,target.y)
      -- Here we don't mind if restricted squares are outside the border, unless it's part of another vault's door.
      -- We don't want to butt up right next to an eligible wall without joining to it (or do we ...? it would mean less chokepoints existing... but could block parts of the scenery)
      if (target_usage == nil or target_usage.usage == "eligible" or (target_usage.usage == "eligible_open" and target_usage.room ~= usage.room)
         or (target_usage.usage == "restricted" and target_usage.reason ~= "vault" and target_usage.room ~= usage.room ) ) then
        -- is_clear = false
        break
      end
    end
  end

  -- No clear space found, the function fails and we'll look for a new spot
  if not is_clear then return false end

  -- Lookup the two corners of the room and map to oriented coords to find the top-leftmost and bottom-rightmost coords relative to dungeon orientation.
  -- TODO: This should only be needed for vault placement and we only need the origin I think
  local c1,c2 = room_base,vaults_vector_add(room_base, { x = room.size.x - 1, y = room.size.y - 1 }, room_final_x_normal, room_final_y_normal)
  local origin, opposite = { x = math.min(c1.x,c2.x), y = math.min(c1.y,c2.y) },{ x = math.max(c1.x,c2.x), y = math.max(c1.y,c2.y) }

  -- Store; will also be used for vault analysis
  room.origin = origin
  room.opposite = opposite

  -- Calculate the final orientation we need for the room to match the door
  -- Somewhat worked out by trial and error but it appears to work
  local final_orient = room_final_y_dir -- (v_normal_dir - orient.dir + 2) % 4 -- Should be same as original room orient?

  -- Place the vault if we have one
  if room.type == "vault" then
    -- We can only rotate a map clockwise or anticlockwise. To rotate 180 we just flip on both axes.
    if final_orient == 0 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,0,true,true)
    elseif final_orient == 1 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,-1,true,true)
    elseif final_orient == 2 then dgn.reuse_map(room.vplace,origin.x,origin.y,true,true,0,true,true)
    elseif final_orient == 3 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,1,true,true) end
  end

  -- Wall type. Originally this was randomly varied but it didn't work well when rooms bordered.
  room.wall_type = options.layout_wall_type

  -- Loop through all coords and paint what we find
  for i,coord in ipairs(coords_list) do
    -- Paint walls
    if coord.room_mask.wall then
      dgn.grid(coord.grid_pos.x, coord.grid_pos.y, room.wall_type)
    -- Draw grid features
    elseif room.type == "grid" then
      if not coord.room_cell.space and coord.room_cell.feature ~= "space" and coord.room_cell.feature ~= nil then
        dgn.grid(coord.grid_pos.x, coord.grid_pos.y, coord.room_cell.feature)
      end
    end
  end

  -- Loop through every square of the room and set wall usage
  -- TODO: Previous loop can be combined with this one?
  local wall_usage, new_depth = "eligible", 2
  if usage.usage == "open" or usage.usage == "eligible_open" then wall_usage = "eligible_open" end
  if usage.depth ~= nil then new_depth = usage.depth + 1 end  -- Room depth

  local wall_orient = orient.dir
  local incidental_connections = { }
  local door_connections = { }
  for i, coord in ipairs(coords_list) do
    local grid_cell = coord.room_cell
    local mask_cell = coord.room_mask

    if mask_cell.vault then
      vaults_set_usage(usage_grid,coord.grid_pos.x,coord.grid_pos.y,{ usage = "restricted", room = room, reason = "vault" })
    elseif mask_cell.empty then
      vaults_set_usage(usage_grid,coord.grid_pos.x,coord.grid_pos.y,{ usage = "empty" })
    elseif mask_cell.wall then

      -- Check if overlapping existing wall
      local grid_coord = coord.grid_pos
      local current_usage = coord.grid_usage

      if usage.usage == "open" and current_usage.usage == "open" and mask_cell.connected then
        -- Count all sides as door connections; potentially we'll get doors on multiple side
        table.insert(door_connections, { room_coord = { x=n,y=m }, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
      end
      if (current_usage.usage == "eligible" and usage.usage == "eligible") or (current_usage.usage == "eligible_open" and usage.usage == "eligible_open") then
      -- Overlapping cells with current room, these are potential door wall candidates
        if usage.room == current_usage.room then
          -- Door connections
          vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = "restricted", room = room, reason = "door" })
          if mask_cell.connected then
            table.insert(door_connections, { room_coord = { x=n,y=m }, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
          end
        else
          -- Incidental connections, we can optionally make doors here
          vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = "restricted", room = room, reason = "cell" })
          if mask_cell.connected then
            if incidental_connections[mask_cell.dir] == nil then incidental_connections[mask_cell.dir] = {} end
            table.insert(incidental_connections[mask_cell.dir], { room_coord = { x=n,y=m }, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
          end
        end
      end
      -- Carving into rock / open space
      if mask_cell.connected and ((current_usage.usage == "none" and usage.usage == "eligible") or (current_usage.usage == "open" and (usage.usage == "eligible_open" or usage.usage == "open"))) then
        local u_wall_dir = (room_final_y_dir + mask_cell.dir) % 4
        vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = wall_usage, normal = normals[u_wall_dir + 1], room = room, depth = new_depth, cell = grid_cell, mask = mask_cell })
      end

    end
  end

    -- TODO: Following logic to be reintegrated into code room generation
  -- local set_empty = crawl.coinflip() -- Allow the room to be used for more rooms or not?
   --   elseif x > origin.x + 1 and x < opposite.x-1 and y > origin.y - 1 and y < opposite.y - 1 then  -- Leaving a 2 tile border
    --    vaults_set_usage(usage_grid,x,y,{usage = "open"})
    --  else
    --    vaults_set_usage(usage_grid,x,y,{usage = "restricted", reason = "border"})
    --  end

  -- How big the door?
  local needs_door = true  -- Only reason we wouldn't need a door is if we're intentionally creating a dead end, not implemented yet
  if needs_door then
    local has_windows = (not room.no_windows) and crawl.one_chance_in(5)

    decorate_walls(room,door_connections,true,has_windows)
    -- Have a chance to add doors / windows to each other side of the room
    for n = 0, 3, 1 do
      if incidental_connections[n] ~= nil and #(incidental_connections[n]) > 0 then
        decorate_walls(room,incidental_connections[n],false,has_windows)
      end
    end

  end

  return true
end
