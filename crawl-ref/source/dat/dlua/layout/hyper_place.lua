------------------------------------------------------------------------------
-- v_place.lua:
--
-- Functions for selecting vaults, generating rooms from code, analysing for
-- geometry and connectability, and finding a placement position on the
-- usage grid.
--
------------------------------------------------------------------------------
-- GENERAL TODO:
-- 2. CAN FIX WITH STRATEGIES: Open room placement is slow. When we place an open room we should set ineligible squares all around it (at e.g. 2 tiles distance). This will reduce
--    attempts to place overlapping rooms and reduce the overall number of 'open' spots, meaning we get increasingly more likely to hit an 'eligible_open' instead.
--    These new restricted zones still need to reference the room so they don't stop proper attachment of new rooms.
-- 3. CAN FIX WITH _process_disconnected_zones: Funny-shaped rooms can still produce VETO-causing bubbles even with all this checking, this can typically happen when two funny-shaped rooms connect
--    via two separate walls, creating an enclosed spot in between. Thankfully this doesn't happen very often (yet). There are two ways to avoid this: a) Relax
--    the VETO restriction, this shouldn't actually be a problem because Crawl automatically generates trapdoors anyway! Crawl could instead / additionally fill the area with
--    no_rtele_into (but on its own this doesn't solve the problem that an off-level hatch/shaft could land the player there, and possibly end them up in an a multi-level
--    hatch loop!)  b) Actually detect and handle such enclosed areas during layout generation. This sounds hard to do in a performance-friendly way but it only needs
--    a check when we're attaching to an open_eligible, need to check each tile that's still open for connectivity to the room we just placed (or some sort of flood fill).
-- 4. General performance - it's definitely ok in V but some of the experimental layouts I've tried have more problems. A big factor is needing to eliminate more eligible
--    spots when we place rooms because once a level is really busy most placement attempts are doomed. When we place a room we should send out rays in the directions
--    of normals and if we hit something close (or map edge) then that wall is no longer eligible. The trigger proximity needs to be 1 less than the smallest room that can fit.
--    Can be improved with custom placement strategies and usage filters. Also it's a good idea to pick a room first rather than picking walls, we can weight more towards
--    rooms with less connections (room_sort/room_pick callbacks)
-- 5. Additional corridor / door carving. Send rays out from walls (in combination with the above) and if a room is within a suitable proximity (e.g. < 5) then we can
--    carve out a corridor and doors. Another strategy is to place all rooms first and then draw paths to other rooms (like roguey).
-- 6. Room flipping. Should be fairly straightforward now; we need to a) invert the x or y normal dependending on which way we're flipping, b) appropriately adjust the logic
--    of vault placement and c) similarly flip the final usage normals. I'm just hesitant to throw this spanner in the works before everything else is 100% working.

hyper.place = {}

------------------------------------------------------------------------------
-- Main public functions.
-- These functions are the main build methods we will use, that get called by
-- items in the build table.

-- Build a series of rooms given a build table
function hyper.place.build_rooms(build,usage_grid,options)

  -- Init local variables
  local results, rooms_placed, times_failed, total_failed, room_count, total_rooms_placed = { }, 0, 0, 0, build.max_rooms or options.max_rooms or 10, options.rooms_placed or 0

  if hyper.profile then
    profiler.push("PlaceRoom", { failed = times_failed })
  end

  local post_place = (build.post_placement_callback or options.post_placement_callback)

  -- Keep trying to place rooms until we've had max_room_tries fail in a row or we reach the max
  while rooms_placed < room_count and times_failed < options.max_room_tries do
    local placed = false

    -- Pick and construct a room
    local room = hyper.rooms.pick_room(build,options)
    -- Attempt to place it. The placement function will try several times to find somewhere to fit it. If it fails
    -- then we generate another room and try again until placement has failed max_room_tries in a row.
    if room ~= nil then
      if room.grid == nil then
        print ("Room grid empty! Pass: "..build.pass..", Generator: " .. room.chosen_generator.generator .. ", " .. (room.chosen_generator.name or room.chosen_generator.tag))
        return results
      end

      local result = hyper.place.place_room(room,build,usage_grid,options)
      if result ~= nil and result.placed then
        placed = true
        -- Increment rooms placed in this build, and total rooms placed by all bulids
        -- TODO: Could increment this automatically when applying instead...
        rooms_placed = rooms_placed + 1
        total_rooms_placed = total_rooms_placed + 1
        options.rooms_placed = total_rooms_placed
        -- Perform analysis for stairs (and perform inner wall substitution if applicable)
        if post_place ~= nil then post_place(usage_grid,room,result,options) end

        table.insert(results,result)
        -- Increment the count of rooms of this type
        if room.generator_used ~= nil then
          if room.generator_used.placed_count == nil then room.generator_used.placed_count = 0 end
          room.generator_used.placed_count = room.generator_used.placed_count + 1
        end

        if hyper.profile then
          profiler.pop()
          profiler.push("PlaceRoom", { failed = total_failed, failed_in_a_row = times_failed })
        end

        times_failed = 0 -- Reset fail count

      end

    end

    if not placed then
      times_failed = times_failed + 1 -- Increment failure count
      total_failed = total_failed + 1
    end

    -- dump_usage_grid_v3(usage_grid)

  end

  if hyper.profile then
    profiler.pop()
    profiler.push("PlacedRooms", { failed = total_failed, placed = rooms_placed })
    profiler.pop()
  end

  -- TODO: Return some useful stats in results (total placements, total failures, so forth)
  return results

end

-- Places a room onto the grid
function hyper.place.place_room(room,build,usage_grid,options)

  local tries = 0
  local done = false

  local state

  if hyper.profile then
    profiler.push("PlaceRoom")
  end

  -- TODO: Can optimise this (and a few other similarly cascading selectors) by copying such options down onto the generator.
  -- TODO: Further to this, could be handy to override individual callbacks in the strategy rather than just the whole thing
  local strategy = room.generator_used.strategy or build.strategy or options.stategy or hyper.place.strategy_default
  if strategy == nil then
    print("No strategy: " .. (build.pass or "[unknown]"))
    return { placed = false }
  end

  -- Have a few goes at placing; we'll select a random eligible spot each time and try to place the vault there
  while tries < options.max_place_tries and not done do
    tries = tries + 1

    -- Let the place picker find us a spot
    local place = strategy.pick_place(room,build,usage_grid,options)
    if not place or place == nil then return { placed = false } end

    -- Now, pick an anchor on the given room
    local anchor = strategy.pick_anchor(place,room,build,usage_grid,options)
    if not anchor or anchor == nil then return { placed = false } end

    -- Attempt to place the room on the wall at a random position
    state = hyper.place.process_room_place(anchor, place, room, strategy, build, usage_grid, options)
    if state and hyper.place.apply_room(state,room,build,usage_grid,options) then
      done = true
    end
  end
  if not done then return { placed = false } end

  if hyper.profile then
    profiler.pop()
  end

  return { placed = done, coords_list = state.coords, state = state }
end

-- For a given position and room orientation determines what grid features this room is overlapping
-- and therefore whether we're allowed to place it (depending on how vetoes etc. are set up)
function hyper.place.process_room_place(anchor, place, room, strategy, build, usage_grid, options)

  local pos,usage,dir,origin = place.pos,place.usage,anchor.dir,anchor.origin

  -- Figure out the mapped x and y vectors of the room relative to its orient
  local room_final_x_dir = (dir - 1) % 4
  local room_final_y_dir = (dir - 2) % 4
  local room_final_x_normal = vector.normals[room_final_x_dir + 1]
  local room_final_y_normal = vector.normals[room_final_y_dir + 1]

  -- Now we can use those vectors along with the position of the connecting wall within the room, to find out where the (0,0) corner
  -- of the room lies on the map (and use that coord for subsequent calculations within the room grid)
  local room_base = vector.add_mapped(pos, { x = -(origin.x), y = -(origin.y) }, room_final_x_normal, room_final_y_normal)

  -- State object to send to callbacks
  local state = {
    anchor = anchor,
    room = room,
    usage = usage,
    pos = pos,
    wall = chosen_wall,
    base = room_base,
    dir = dir,
    build = build,
    options = options,
    normals = {
      x = room_final_x_normal,
      y = room_final_y_normal
    }
  }

  -- Layout configuration can now veto this room placement with a callback
  local veto_place = options.veto_place_callback or strategy.veto_place
  if veto_place ~= nil then
    local result = veto_place(state,usage_grid)
    if result == true then return false end
  end

  -- Loop through all coords of the room and its walls. Map to grid coords and construct a list of relevant coords from this,
  -- to make it really fast to loop through on subsequent occasions.
  local coords_list = {}
  state.coords = coords_list

  local is_clear = true
  local place_check = room.generator_used.veto_cell or build.veto_cell or build.strategy.veto_cell or options.veto_cell or hyper.place.cell_veto_normal
  for m = 0, room.size.y-1, 1 do
    for n = 0, room.size.x-1, 1 do
      local coord = { room_pos = { x = n, y = m } }
      coord.grid_pos = vector.add_mapped(room_base, coord.room_pos, room_final_x_normal, room_final_y_normal)
      coord.grid_usage = hyper.usage.get_usage(usage_grid, coord.grid_pos.x, coord.grid_pos.y)
      coord.room_cell = hyper.usage.get_usage(room.grid,coord.room_pos.x,coord.room_pos.y)

      -- print ("Checking: " .. n .. ", " .. m .. " @ " .. coord.grid_pos.x .. ", " .. coord.grid_pos.y )

      -- Out of bounds is instant fail, although even this we *could* override sometimes if we're careful...
      -- TODO: This is actually checked in cell_veto_standard, we don't need this now right?
      if (not coord.room_cell.space or coord.room_cell.buffer) and coord.grid_usage == nil then
        is_clear = false
        break
      end

      if place_check ~= nil and place_check(coord, state) then
        is_clear = false
        break
      end

      -- Remember non-space and non-vault coords for later. Our veto function might have allowed this room to be placed
      -- despite a vault already existing but we definitely don't want to consider it valid for overwriting later.
      -- TODO: Actually we could use space cells to update usage later to implement buffers
      if not coord.room_cell.space and not coord.grid_usage.vault then
        table.insert(coords_list, coord)
      end
    end
    if not is_clear then break end
  end
  -- Something vetoed this position; the function fails and we'll look for a new spot
  if not is_clear then
    return false
  end

  return state
end

-- Actually apply the room to the dungeon and update the usage grid
function hyper.place.apply_room(state,room,build,usage_grid,options)
  -- Unpack variables
  local usage,coords_list,room_base,final_orient = state.usage,state.coords,state.base,state.dir

  -- Wall type. Originally this was randomly varied but it didn't work well when rooms bordered.
  -- TODO: Wall type should be handled in transform / decorator / post-process ...
  room.wall_type = options.layout_wall_type or "rock_wall"

  -- Now ... loop through every square of the room; update the dungeon grid and wall usage table, and make a list
  -- of which connectable walls overlap with another vault (Note: we should have already made that list during
  -- placement processing because it might be relevant for vetos.)
  -- These walls can be the original orientation wall, 'incidental' walls where the room just
  -- happened to overlap another room's eligible walls, or for open rooms this will be all sides. Then we
  -- pass the lists to decorator functions for painting doors and so forth.
  -- We don't have to end up with a door on the original orientation spot, although we will force that at least
  -- one cell from that wall gets carved, to ensure connectivity.

  -- Room depth
  local new_depth = 2
  if usage.depth ~= nil then new_depth = usage.depth + 1 end

  -- Setup door tables
  local incidental_connections = { }
  for n = 0, 3, 1 do
    incidental_connections[n] = {}
  end
  local door_connections = { }

  for i, coord in ipairs(coords_list) do

    -- A vault will be placed here then fill the area with rock. This is so that inner empty space will be rock-filled
    -- even in open areas.
    -- TODO: For this to work we'd have to have already figured out which empty areas were enclosed.
    -- if room.type == "vault" or (room.type == "transform" and coord.room_cell.vault) then
      -- dgn.grid(coord.grid_pos.x, coord.grid_pos.y, room.wall_type)
    -- end
    -- Apply the specific feature onto the dungeon grid.
    -- TODO: We shouldn't even perform this or apply the usage yet. Let wall decorators operate first (which themselves
    -- will also update usage) and only finally apply the entire grid once we're certain what goes there.
    if (room.type == "grid" or room.type == "transform") and not coord.room_cell.space and not coord.room_cell.vault and not options.test then
      dgn.grid(coord.grid_pos.x, coord.grid_pos.y, coord.room_cell.feature)
    end

    local room_cell,grid_cell,grid_coord = coord.room_cell,coord.grid_usage,coord.grid_pos
    -- Update directions
    -- TODO: Should ensure cells always have anchors then following line isn't needed
    if room_cell.anchors == nil then room_cell.anchors = {} end
    for a,anchor in ipairs(room_cell.anchors) do
      local u_wall_dir = (anchor.normal.dir + final_orient) % 4
      anchor.normal = vector.normals[u_wall_dir+1]
      anchor.grid_pos = vector.add_mapped(state.base, anchor.pos, state.normals.x, state.normals.y)
    end
    -- Manage overlays
    -- TODO: There is a bunch of stuff we've lost here and need to reinstate....
    if room_cell.carvable then
      if usage.open_area or not usage.solid then room_cell.open_area = true end
      local wall_info = { room_coord = coord.room_pos, grid_pos = grid_coord, usage = grid_usage, cell = room_cell }
      if grid_cell.carvable then
        -- Once two carvables have overlayed, we should not carve any more
        room_cell.carvable = false
        room_cell.restricted = true
        if grid_cell.room == usage.room then
          -- Original wall
          table.insert(door_connections, wall_info)
        elseif room_cell.anchors ~= nil and room_cell.anchors[1] ~= nil then
          -- Incidental overlay
          table.insert(incidental_connections[room_cell.anchors[1].normal.dir], wall_info)
        end
      else
        -- Count all open sides as door connections; potentially we'll get doors on multiple side
        if not grid_cell.solid then
          table.insert(door_connections, wall_info)
        end
      end
    end

    -- Update usage
    room_cell.depth = new_depth
    room_cell.room = room
    room_cell.room_dir = final_orient
    hyper.usage.set_usage(usage_grid,coord.grid_pos.x,coord.grid_pos.y,room_cell)
  end

  -- Place a vault map if we have one
  local vault_room, vault_bounds
  if room.type == "vault" then
    vault_room = room
    vault_pos = { x = 0, y = 0 }
  elseif room.type == "transform" and room.inner_room.type == "vault" then
    vault_room = room.inner_room
    vault_pos = room.inner_pos
  end

  if options.test then return coords_list end

  if vault_room ~= nil then
    -- Lookup the two corners of the room and map to oriented coords to find the top-leftmost and bottom-rightmost coords relative to dungeon orientation.
    local v1,v2 = vault_pos,{ x = vault_pos.x + vault_room.size.x - 1, y = vault_pos.y + vault_room.size.y - 1 }

    local c1 = vector.add_mapped(room_base,v1,state.normals.x, state.normals.y)
    local c2 = vector.add_mapped(room_base,v2,state.normals.x, state.normals.y)

    -- Vault origin found by min coords
    local origin = { x = math.min(c1.x,c2.x), y = math.min(c1.y,c2.y) }

    -- We can only rotate a map clockwise or anticlockwise. To rotate 180 we just flip on both axes.
    if final_orient == 0 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,0,true,true)
    elseif final_orient == 1 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,-1,true,true)
    elseif final_orient == 2 then dgn.reuse_map(room.vplace,origin.x,origin.y,true,true,0,true,true)
    elseif final_orient == 3 then dgn.reuse_map(room.vplace,origin.x,origin.y,false,false,1,true,true) end
  end

  -- Use decorator callbacks to decorate the connector cells; e.g. doors and windows, solid wall, open wall ...
  local decorate_callback = build.wall_decorator or options.decorate_walls_callback or hyper.rooms.decorate_walls
  decorate_callback(state,door_connections,true)
  -- Have a chance to add doors / windows to each other side of the room
  for n = 0, 3, 1 do
    if incidental_connections[n] ~= nil and #(incidental_connections[n]) > 0 then
      decorate_callback(state,incidental_connections[n],false)
    end
  end

  return coords_list
end

-- TODO: This callback only actually applies to V, right now at least. It's reference as a callback in vaults_default_options and should
-- move to hyper_vaults along with anything else V-specific prior to merge ...?
function hyper.place.analyse_vault_post_placement(usage_grid,room,result,options)
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

-- TODO: This is the post-everything function for V so we can be very
-- specific about how stairs are handled. It probably shouldn't be in
-- this include since it relies on the post-placement-analysis from V
-- and typically we want to let stairs place randomly anyway.
function hyper.place.restore_stairs(results, usage_grid, options)

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

  return true
end
