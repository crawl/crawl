------------------------------------------------------------------------------
-- v_rooms.lua:
--
-- This file handles the main task of selecting rooms and drawing them onto
-- the layout.
------------------------------------------------------------------------------

hypervaults.rooms = {}

-- Moves from a start point along a move vector mapped to actual vectors xVector/yVector
-- This allows us to deal with coords independently of how they are rotated to the dungeon grid
local function vaults_vector_add(start, move, xVector, yVector)

  return {
    x = start.x + (move.x * xVector.x) + (move.y * yVector.x),
    y = start.y + (move.x * xVector.y) + (move.y * yVector.y)
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

    -- TODO: Following logic to be reintegrated into code room generation; meaning large empty rooms could have other rooms placed inside them again
  -- local set_empty = crawl.coinflip() -- Allow the room to be used for more rooms or not?/
   --   elseif x > origin.x + 1 and x < opposite.x-1 and y > origin.y - 1 and y < opposite.y - 1 then  -- Leaving a 2 tile border
    --    vaults_set_usage(usage_grid,x,y,{usage = "open"})
    --  else
    --    vaults_set_usage(usage_grid,x,y,{usage = "restricted", reason = "border"})
    --  end

local function make_code_room(options,chosen)
  -- Pick a size for the room
  local size
  if chosen.pick_size_callback ~= nil then
    size = chosen.pick_size_callback(options)
  else
    local min_size, max_size = options.min_room_size,options.max_room_size
    if chosen.min_size ~= nil then min_size = chosen.min_size end
    if chosen.max_size ~= nil then max_size = chosen.max_size end
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
  paint_grid(chosen.paint_callback(room,options,chosen),options,room.grid)
  if _VAULTS_DEBUG then dump_layout_grid(room.grid) end
  if chosen.decorate_callback ~= nil then chosen.decorate_callback(room.grid,room,options) end  -- Post-production

  return room
end

-- Pick a map by tag and make a room grid from it
local function make_tagged_room(options,chosen)
  -- Resolve a map with the specified tag
  local mapdef = dgn.map_by_tag(chosen.tag,true)
  if mapdef == nil then return nil end  -- Shouldn't happen when thing are working but just in case

  -- Temporarily prevent map getting mirrored / rotated during resolution because it'll seriously
  -- screw with our attempts to understand and position the vault later; and hardwire transparency because lack of it can fail a whole layout
  -- TODO: Store these tags on the room first so we can actually support them down the line ...
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
  -- Check min/max room sizes are observed
  if (chosen.min_size == nil or not (room_width < chosen.min_size or room_height < chosen.min_size))
    and (chosen.max_size == nil or not (room_width > chosen.max_size or room_height > chosen.max_size)) then
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
      if dgn.has_tag(room.map,"vaults_orient_" .. hypervaults.normals[n+1].name) then
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
      local has_exits = room.has_orient and true or false
      for m = -1, room.size.y, 1 do
        room.mask_grid[m] = {}
        for n = -1, room.size.x, 1 do
          local cell = vaults_get_layout(room.grid,n,m)

          if cell.exit then has_exits = true end

          if not cell.space then
            --if not cell.solid then -- TODO: Check for wall (this is NOT the same as 'solid')
              room.mask_grid[m][n] = { vault = true, wall = false, space = false }
            --else
            -- room.mask_grid[m][n] = { vault = true, wall = true, space = false }

          else
            -- Analyse layout squares around it
            -- TODO: Existing wall cells in the vault can be considered space for this purpose, it'll stop us getting a double layer of wall around some vaults
            local all_space = true
            for ax = n-1, n+1, 1 do
              for ay = m-1, m+1, 1 do
                if ax ~= n or ay ~= m then
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

      -- Loop through the cells again now we know has_exits, and store all connected cells in our list of walls for later.
      for m = -1, room.size.y, 1 do
        for n = -1, room.size.x, 1 do
          local wall_mask = room.mask_grid[m][n]
          if wall_mask.connected then
            local cell = wall_mask.exit_cell
            local is_open = true
            if cell.space or (has_exits and not cell.exit and not (room.has_orient and room.tags[wall_mask.dir])) then
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

  if _VAULTS_DEBUG then
    dump_mask_grid(room)
  end

  return room

end

-- TODO: This callback only actually applies to V, right now at least. Should specify it in the config instead of always calling.
local function analyse_vault_post_placement(usage_grid,room,result,options)
  local perform_subst = true
  if room.preserve_wall or room.wall_type == nil then perform_subst = false end
  result.stairs = { }
  for i,coord in ipairs(result.coords_list) do
    p = coord.grid_pos
    if dgn.in_bounds(p.x,p.y) and
      (feat.is_stone_stair(p.x,p.y)) or
      -- On V:1 the branch entrant stairs don't count as stone_stair; we need to check specifically for the V exit stairs
      -- to avoid overwriting e.g. Crypt or Blade entrance stairs!
      dgn.feature_name(dgn.grid(p.x,p.y)) == "exit_vaults" then
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
        -- Perform analysis for stairs (and perform inner wall substituion if applicable)
        analyse_vault_post_placement(data,room,result,options)
        table.insert(results,result)
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
    "stone_stairs_up_ii", "stone_stairs_up_iii" }

    -- We could place some hatches in rooms but from discussion on IRC just let them place naturally -
    -- they inherently carry some risk and can't be used for pulling tactics so it's fine to have them in corridors.
    -- "escape_hatch_up", "escape_hatch_down",
    -- "escape_hatch_up", "escape_hatch_down" }

  for n,stair_type in ipairs(stair_types) do
    -- Do we have any left?
    if #stairs == 0 then break end
    -- Any random stair
    local i = crawl.random_range(1, #stairs)
    local stair = stairs[i]
    -- Place it
    dgn.grid(stair.pos.x, stair.pos.y, stair_type)
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
  local list

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
    list = vaults_maybe_place_vault(e, spot, usage_grid, usage, room, options)
    if list ~= nil and list ~= false then
      done = true
    end
  end
  return { placed = done, coords_list = list }
end

function hypervaults.rooms.decorate_walls_open(state, connections, door_required, has_windows)

  for i, door in ipairs(connections) do
    dgn.grid(door.grid_pos.x,door.grid_pos.y, "floor")
  end

end

function hypervaults.rooms.decorate_walls(state, connections, door_required, has_windows)

  -- TODO: Use decorator callbacks for these so we can vary by layout
  local have_door = true
  if not door_required then
    have_door = crawl.x_chance_in_y(4,5)
  end

  if have_door then
    -- Usually we place one door on a wall
    local num_doors = 1
    -- Sometimes have a chance to place more
    if crawl.one_chance_in(3) then
      -- Although this still might pick only 1
      num_doors = math.abs(crawl.random2avg(9,4)-4)+1  -- Should tend towards 1 but rarely can be up to 5
    end
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
  if has_windows and not state.room.no_windows and crawl.one_chance_in(14) then
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

-- Attempts to place a room by picking a random available spot and checking it'll fit.
-- GENERAL TODO:
-- 1. This function is massive and could do with breaking up into a few parts (calculate normals; check space / generate coords; paint and set usage)
-- 2. Open room placement is slow. When we place an open room we should set ineligible squares all around it (at e.g. 2 tiles distance). This will reduce
--    attempts to place overlapping rooms and reduce the overall number of 'open' spots, meaning we get increasingly more likely to hit an 'eligible_open' instead.
--    These new restricted zones still need to reference the room so they don't stop proper attachment of new rooms.
-- 3. Funny-shaped rooms can still produce VETO-causing bubbles even with all this checking, this can typically happen when two funny-shaped rooms connect
--    via two separate walls, creating an enclosed spot in between. Thankfully this doesn't happen very often (yet). There are two ways to avoid this: a) Relax
--    the VETO restriction, this shouldn't actually be a problem because Crawl automatically generates trapdoors anyway! Crawl could instead / additionally fill the area with
--    no_rtele_into (but on its own this doesn't solve the problem that an off-level hatch/shaft could land the player there, and possibly end them up in an a multi-level
--    hatch loop!)  b) Actually detect and handle such enclosed areas during layout generation. This sounds hard to do in a performance-friendly way but it only needs
--    a check when we're attaching to an open_eligible, need to check each tile that's still open for connectivity to the room we just placed (or some sort of flood fill).
-- 4. General performance - it's definitely ok in V but some of the experimental layouts I've tried have more problems. A big factor is needing to eliminate more eligible
--    spots when we place rooms because once a level is really busy most placement attempts are doomed. When we place a room we should send out rays in the directions
--    of normals and if we hit something close (or map edge) then that wall is no longer eligible. The trigger proximity needs to be 1 less than the smallest room that can fit.
--    This should vastly help.
-- 5. Additional corridor / door carving. Send rays out from walls (in combination with the above) and if a room is within a suitable proximity (e.g. < 5) then we can
--    carve out a corridor and doors.
-- 6. Room flipping. Should be fairly straightforward now; we need to a) invert the x or y normal dependending on which way we're flipping, b) appropriately adjust the logic
--    of vault placement and c) similarly flip the final usage normals. I'm just hesitant to throw this spanner in the works before everything else is 100% working.
function vaults_maybe_place_vault(e, pos, usage_grid, usage, room, options)
  local v_normal, v_wall, v_normal_dir,wall

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

  -- Pick a random open cell on the wall. This is where we'll attach the room to the door.
  if #wall == 0 then return false end -- Shouldn't happen, eligible should mean cells available
  local chosen_wall = wall[crawl.random2(#wall)+1]

  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  -- In open space we can pick a random normal for the door
  if usage.usage == "open" then
    -- Pick a random rotation for the door wall
    v_normal = hypervaults.normals[crawl.random_range(1,4)]
    v_normal_dir = v_normal.dir
  end

  -- If placing a room in an existing wall we have the normal stored alredy in the usage data
  if usage.usage == "eligible" or usage.usage == "eligible_open" then
    v_normal = usage.normal
    v_normal_dir = usage.normal.dir
    -- TODO: make sure usage.normal.dir is never nil and remove this loop
    if v_normal_dir == nil then
      for i,n in ipairs(hypervaults.normals) do
        if n.x == v_normal.x and n.y == v_normal.y then
          v_normal_dir = n.dir
        end
      end
    end
  end

  -- Figure out the mapped x and y vectors of the room relative to its orient
  local room_final_x_dir = (v_normal_dir + 1 - chosen_wall.dir) % 4
  local room_final_y_dir = (room_final_x_dir - 1) % 4
  local room_final_x_normal = hypervaults.normals[room_final_x_dir + 1]
  local room_final_y_normal = hypervaults.normals[room_final_y_dir + 1]

  -- Calculate how much the room will have to rotate to match the new orientation
  local final_orient = (room_final_x_dir + 1) % 4

  -- Now we can use those vectors along with the position of the connecting wall within the room, to find out where the (0,0) corner
  -- of the room lies on the map (and use that coord for subsequent calculations within the room grid)
  local room_base = vaults_vector_add(pos, { x = -(chosen_wall.pos.x), y = -(chosen_wall.pos.y) }, room_final_x_normal, room_final_y_normal)

  -- State object to send to callbacks
  local state = {
    room = room,
    usage = usage,
    pos = pos,
    wall = chosen_wall,
    base = room_base,
    dir = final_orient,
    usage_grid = usage_grid
  }

  -- Layout configuration can now veto this room placement with a callback
  if options.veto_place_callback ~= nil then
    local result = options.veto_place_callback(state)
    if result == true then return false end
  end

  -- Loop through all coords of the room and its walls. Map to grid coords and add all this data into a list to make iterating through
  -- map squares easier in the future
  local coords_list = {}
  local is_clear = true

  for m = -1, room.size.y, 1 do
    for n = -1, room.size.x, 1 do
      local coord = { room_pos = { x = n, y = m } }
      coord.grid_pos = vaults_vector_add(room_base, coord.room_pos, room_final_x_normal, room_final_y_normal)
      coord.grid_usage = vaults_get_usage(usage_grid, coord.grid_pos.x, coord.grid_pos.y)
      -- Out of bounds (maybe we should catch this earlier by checking both corners)
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
          or (usage.usage == "eligible" and target_usage.usage ~= "eligible" and target_usage.usage ~= "none")
          or (usage.usage == "open" and target_usage.usage ~= "open")
          or (usage.usage == "eligible_open" and target_usage.usage ~= "open"
             and not ((target_usage.usage == "eligible_open" or (target_usage.usage == "restricted" and target_usage.reason == "wall")) and usage.room == target_usage.room)))
             then
          is_clear = false
          break
        end

        -- For open rooms, check that the only _nearby_ room (within 2 tiles) is the one we are supposed to be attaching to.
        -- This prevents us blocking off doors of rooms that just happened to be nearby, and also prevents disconnected layouts because
        -- if open rooms connect in non-tree-like fashion it produces enclosed bubbles that are extremely difficult to either detect
        -- or fix. (It would be nice to do something about this but probably too difficult to be worth it right now).
        -- As it stands this is not highly optimal and could certainly be improved.
        if (usage.usage == "open" or usage.usage == "eligible_open") and coord.room_mask.wall then
          for mN = -1, 1, 1 do
            for nN = -1, 1, 1 do
              local posN = { x = coord.room_pos.x + nN, y = coord.room_pos.y + mN }
              local cellN = vaults_get_layout(room.grid,posN.x,posN.y)
              local maskN = vaults_get_layout(room.mask_grid,posN.x,posN.y)
              if maskN == nil or not (maskN.wall or maskN.vault) then
                local gridN = vaults_vector_add(room_base, posN, room_final_x_normal, room_final_y_normal)
                local usageN = vaults_get_usage(usage_grid, gridN.x, gridN.y)
                if (usageN.usage ~= "open" and not (usageN.usage == "restricted" and usageN.reason == "border") and (usageN.room == nil or usageN.room ~= usage.room)) then -- and (usageN.usage == "vault" or usageN.usage == "eligible_open"))) then
                  is_clear = false
                end
              end
            end
          end
          if not is_clear then break end
        end

        table.insert(coords_list, coord)
      end
    end
    if not is_clear then break end
  end

  -- No clear space found, the function fails and we'll look for a new spot
  if not is_clear then return false end

  -- Lookup the two corners of the room and map to oriented coords to find the top-leftmost and bottom-rightmost coords relative to dungeon orientation.
  -- TODO: This should only be needed for vault placement and we only need the origin I think
  local c1,c2 = room_base,vaults_vector_add(room_base, { x = room.size.x - 1, y = room.size.y - 1 }, room_final_x_normal, room_final_y_normal)
  local origin = { x = math.min(c1.x,c2.x), y = math.min(c1.y,c2.y) }

  -- Store; will also be used for vault analysis (?)
  room.origin = origin

  -- Wall type. Originally this was randomly varied but it didn't work well when rooms bordered.
  room.wall_type = options.layout_wall_type

  -- Now loop through every square of the room; update the dungeon grid and wall usage table, and make a list
  -- of connectable walls - i.e. walls with open space on both side; they could be the original orientation wall,
  -- 'incidental' walls where the room just happened to overlap another room's eligible walls, or for open rooms
  -- this will be all sides. The wall lists will get passed to decorator functions for painting doors and so forth.
  -- We don't have to end up with a door on the original orientation spot, although we will ensure that at least
  -- one cell from that wall gets carved, to ensure connectivity.
  local wall_usage, new_depth = "eligible", 2
  if usage.usage == "open" or usage.usage == "eligible_open" then wall_usage = "eligible_open" end
  if usage.depth ~= nil then new_depth = usage.depth + 1 end  -- Room depth

  local incidental_connections = { }
  local door_connections = { }
  local open_connections = { }
  for i, coord in ipairs(coords_list) do

    -- Paint walls
    if coord.room_mask.wall then
      dgn.grid(coord.grid_pos.x, coord.grid_pos.y, room.wall_type)
    -- Draw grid features (for code rooms)
    elseif room.type == "grid" then
      if not coord.room_cell.space and coord.room_cell.feature ~= "space" and coord.room_cell.feature ~= nil then
        dgn.grid(coord.grid_pos.x, coord.grid_pos.y, coord.room_cell.feature)
      end
    end

    -- Handle usage
    local grid_cell = coord.room_cell
    local mask_cell = coord.room_mask

    if mask_cell.vault then
      if grid_cell.empty then
        vaults_set_usage(usage_grid,coord.grid_pos.x,coord.grid_pos.y,{ usage = "empty" })
      else
        vaults_set_usage(usage_grid,coord.grid_pos.x,coord.grid_pos.y,{ usage = "restricted", room = room, reason = "vault" })
      end

    elseif mask_cell.wall then

      -- Check if overlapping existing wall
      local grid_coord = coord.grid_pos
      local current_usage = coord.grid_usage

      if current_usage.usage == "open" and mask_cell.connected then
        -- Count all sides as door connections; potentially we'll get doors on multiple side
        table.insert(open_connections, { room_coord = coord.room_pos, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
      end
      if (current_usage.usage == "eligible" and usage.usage == "eligible") or (current_usage.usage == "eligible_open" and usage.usage == "eligible_open") then
      -- Overlapping cells with current room, these are potential door wall candidates
        if usage.room == current_usage.room then
          -- Door connections
          vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = "restricted", room = room, reason = "door" })
          if mask_cell.connected then
            table.insert(door_connections, { room_coord = coord.room_pos, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
          end
        else
          -- Incidental connections, we can optionally make doors here
          vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = "restricted", room = room, reason = "cell" })
          if mask_cell.connected then
            if incidental_connections[mask_cell.dir] == nil then incidental_connections[mask_cell.dir] = {} end
            table.insert(incidental_connections[mask_cell.dir], { room_coord = coord.room_pos, grid_pos = grid_coord, usage = current_usage, mask = mask_cell })
          end
        end
      end
      -- Carving into rock / open space
      if mask_cell.connected and ((current_usage.usage == "none" and usage.usage == "eligible") or (current_usage.usage == "open" and (usage.usage == "eligible_open" or usage.usage == "open"))) then
        local u_wall_dir = (mask_cell.dir + final_orient) % 4
        vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = wall_usage, normal = hypervaults.normals[u_wall_dir + 1], room = room, depth = new_depth, cell = grid_cell, mask = mask_cell })
      elseif not mask_cell.connected and (current_usage.usage == "open" and (usage.usage == "eligible_open" or usage.usage == "open")) then
        -- Should prevent nearby rooms from thinking this space is open
        vaults_set_usage(usage_grid,grid_coord.x,grid_coord.y,{ usage = "restricted", room = room, depth = new_depth, cell = grid_cell, mask = mask_cell, reason = "wall" })
      end
    end
  end

  -- Place the vault if we have one
  if room.type == "vault" then
    -- We can only rotate a map clockwise or anticlockwise. To rotate 180 we just flip on both axes.
    if final_orient == 0 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,0,true,true)
    elseif final_orient == 1 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,-1,true,true)
    elseif final_orient == 2 then dgn.reuse_map(room.vplace,origin.x,origin.y,true,true,0,true,true)
    elseif final_orient == 3 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,1,true,true) end
  end

  -- Use decorator callbacks to decorate the connector cells; e.g. doors and windows, solid wall, open wall ...
  local decorate_callback = options.decorate_walls_callback
  if decorate_callback == nil then decorate_callback = hypervaults.rooms.decorate_walls end

  if #open_connections > 0 then decorate_callback(state,open_connections,true,true) end
  if #door_connections > 0 then decorate_callback(state,door_connections,true,true) end

  -- Have a chance to add doors / windows to each other side of the room
  for n = 0, 3, 1 do
    if incidental_connections[n] ~= nil and #(incidental_connections[n]) > 0 then
      decorate_callback(state,incidental_connections[n],false,true)
    end
  end

  return coords_list
end
