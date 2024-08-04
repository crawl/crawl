------------------------------------------------------------------------------
-- v_rooms.lua:
--
-- Functions to code-generate some common standard types of room.
------------------------------------------------------------------------------

hyper.rooms = {}

-- Picks a generator from a weighted table of generators and builds a room with it
function hyper.rooms.pick_room(build,options)

  if hyper.profile then
    profiler.push("PickRoom")
  end

  -- This callback filters out generators based on standard options which we support for all generators
  local function weight_callback(generator)
    -- max_rooms controls how many rooms _this_ generator can place before it stops
    if generator.max_rooms ~= nil and generator.placed_count ~= nil and generator.placed_count >= generator.max_rooms then
      return 0
    end
    -- min_total_rooms means this many rooms have to have been placed by _any_ generator before this one comes online
    if generator.min_total_rooms ~= nil and options.rooms_placed < generator.min_total_rooms then return 0 end
    -- max_total_rooms stops this generator after this many rooms have been placed by any generators
    if generator.max_total_rooms ~= nil and options.rooms_placed >= generator.max_total_rooms then return 0 end
    -- Generator can have a weight function instead of a flat weight
    if generator.weight_callback ~= nil then return generator.weight_callback(generator,options) end
    -- Otherwise use the generator's default weight
    return generator.weight
  end

  local weights = build.generators or options.room_type_weights

  -- Pick generator from weighted table
  local chosen = util.random_weighted_from(weight_callback,weights)
  local room

  -- Main generator loop
  local veto,tries,maxTries = false,0,50
  while tries < maxTries and (room == nil or veto) do
    tries = tries + 1
    veto = false

    room = hyper.rooms.make_room(options,chosen)

    -- Allow veto callback to throw this room out (e.g. on size, exits)
    -- TODO: This veto is still unused and I don't have a use case, maybe should delete it.
    if options.veto_room_callback ~= nil then
      veto = options.veto_room_callback(room)
    end

  end

  if hyper.profile then
    profiler.pop({ tries = tries })
  end

  return room
end

-- Makes a room object from a generator table. Optionally perform analysis
-- (default true) to determine open and connectable squares.
function hyper.rooms.make_room(options,generator)

  if hyper.profile then
    profiler.push("MakeRoom")
  end

  -- Code rooms
  if generator.generator == "code" then
    room = hyper.rooms.make_code_room(generator,options)

  -- Pick vault map by tag
  elseif generator.generator == "tagged" then
    room = hyper.rooms.make_tagged_room(generator,options)
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("AnalyseRoom")
  end

  if room == nil then return nil end

  -- Do we analyse this room? Usually necessary but we might not need
  -- it e.g. random placement layouts where it doesn't matter so much
  local analyse = true
  if generator.analyse ~= nil then analyse = generator.analyse end

  if analyse then
    hyper.rooms.analyse_room(room,options)
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("TransformRoom")
  end

  -- Optionally transform the room. This is used to add walls for V
  -- and other layouts but a transform doesn't have to be provided.
  local transform = generator.room_transform or options.room_transform
  if transform ~= nil then
    room = transform(room,options)
    if perform_analysis then
      hyper.rooms.analyse_room(room,options)
    end
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("AnalyseRoomInternal")
  end

  if hyper.profile then
    profiler.pop()
  end

  if hyper.debug then
    dump_usage_grid_v3(room.grid)
  end

  -- Assign the room an id. This is useful later on when we carve doors etc. so we can identify which walls belong to which rooms.
  -- TODO: The id could instead be incremented for each attempt at placing a room. We can hold onto rooms that failed to place and try
  --       them again later on in case space has opened up. Not sure if this is a good idea but it could save some cycles since
  --       generating rooms is one of the more expensive operations now. Really before making any assumptions like that I should run
  --       some proper analysis of the expensiveness of different operations and get a good picture of what needs to be optimised.
  --       Perhaps it's still possible to find some ways to move expensive parts out to C++ (e.g. mask and normal generation, placement itself)
  --       whilst retaining callbacks.
  room.id = options.rooms_placed

  return room
end

function hyper.rooms.make_code_room(chosen,options)
  -- Pick a size for the room
  local size
  if chosen.size ~= nil then
    if type(chosen.size) == "function" then
      -- Callback function
      size = chosen.size(options,chosen)
    else
      -- Static size
      size = chosen.size
    end
  else
    -- Default size function
    local size_func = chosen.size_callback or hyper.rooms.size_default
    size = size_func(chosen,options)
  end

  room = {
    type = "grid",
    size = size,
    generator_used = chosen,
    grid = hyper.usage.new_usage(size.x,size.y)
  }

  -- Paint and decorate the layout grid
  hyper.paint.paint_grid(chosen.paint_callback(room,options,chosen),options,room.grid)
  if chosen.decorate_callback ~= nil then chosen.decorate_callback(room.grid,room,options) end  -- Post-production

  return room
end

-- Pick a map by tag and make a room grid from it
function hyper.rooms.make_tagged_room(chosen,options)
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
      grid = hyper.usage.new_usage(room_width,room_height)
    }

    room.preserve_wall = dgn.has_tag(room.map, "no_wall_fixup")
    room.no_windows = dgn.has_tag(room.map, "no_windows")

    -- Check all four directions for orient tag before we create the wals data, since the existence of a
    -- single orient tag makes the other walls ineligible
    room.tags = {}
    for n = 0, 3, 1 do
      if dgn.has_tag(room.map,"vaults_orient_" .. vector.normals[n+1].name) then
        room.has_orient = true
        room.tags[n] = true
      end
    end
    -- Make usage grid by feature inspection; will be used to compute wall data
    for m = 0, room.size.y - 1, 1 do
      for n = 0, room.size.x - 1, 1 do
        local inspected = { }
        inspected.feature, inspected.exit, inspected.space = dgn.inspect_map(vplace,n,m)
        inspected.solid = not (feat.has_solid_floor(feature) or feat.is_door(feature))
        inspected.feature = dgn.feature_name(inspected.feature)
        hyper.usage.set_usage(room.grid,n,m,inspected)
      end
    end
    found = true
  end
  return room
end

-- Analyse the exit squares of a room to determine connectability / eligibility for placement.
function hyper.rooms.analyse_room(room,options)
  -- Initialise walls table
  room.walls = { }
  for n = 0, 3, 1 do
    room.walls[n] = { eligible = false }
  end

  local inspect_cells = {}
  local has_exits = false

  if hyper.profile then
    profiler.push("FirstPass")
  end

  for m = 0, room.size.y-1, 1 do
    for n = 0, room.size.x-1, 1 do
      local cell = hyper.usage.get_usage(room.grid,n,m)

      -- If we find an exit then flag this, causing non-exit edge cells to get ignored
      if cell.exit then has_exits = true end
      cell.anchors = {}
      -- Remember if the cell is a relevant connector cell
      if not cell.space and (cell.carvable or not cell.solid) then
        table.insert(inspect_cells,{ cell = cell, pos = { x = n, y = m } } )
      end
    end
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("SecondPass")
  end

  -- Loop through the cells again now we know has_exits, and store all connected cells in our list of walls for later.
  for i,inspect in ipairs(inspect_cells) do
    local cell = inspect.cell
    local n,m = inspect.pos.x,inspect.pos.y

    if not has_exits or cell.exit then
      -- Analyse squares around it
      for i,normal in ipairs(room.allow_diagonals and vector.directions or vector.normals) do
        local near_pos = { x = n + normal.x,y = m + normal.y }
        local near_cell = hyper.usage.get_usage(room.grid,near_pos.x,near_pos.y)
        if near_cell == nil or near_cell.space then
          -- This is a possible anchor
          local anchor = true
          local target = inspect.pos
          local anchor_pos = normal
          -- If it's a wall we need space on the opposite side
          if cell.solid then
            anchor = false
            if cell.carvable then
              local target_cell = hyper.usage.get_usage(room.grid,normal.x - n,normal.y - m)
              if target_cell ~= nil and not target_cell.solid then
                anchor_pos = { x = 0, y = 0 }
                anchor = true
              end
            end
          end
          if anchor then
            -- Record this is as a possible anchor
            table.insert(cell.anchors, { normal = normal, pos = anchor_pos, origin = inspect.pos })
            -- table.insert(room.walls[normal.dir],{ cell = cell, pos = inspect.pos, normal = normal } )
            -- room.walls[normal.dir].eligible = true
            cell.connected = true
         end
        end
      end
      hyper.usage.set_usage(room.grid,n,m,cell)
    end
  end

  if hyper.profile then
    profiler.pop()
  end
end

-- Transforms a room by returning a new room sized 2 bigger with walls added next to all open edge squares
function hyper.rooms.add_walls(room, options)

  local walled_room = {
    type = "transform",
    size = { x = room.size.x + 2, y = room.size.y + 2 },
    generator_used = room.generator_used,
    transform = "add_walls",
    flags = room.flags,
    inner_room = room,
    inner_room_pos = { x = 1, y = 1 }
  }

  walled_room.grid = hyper.usage.new_usage(walled_room.size.x,walled_room.size.y)
  -- Loop through all the squares and add the walls
  for m = 0, walled_room.size.y-1, 1 do
    for n = 0, walled_room.size.x-1, 1 do
      -- Get corresponding square from original room
      local usage = hyper.usage.get_usage(room.grid,n-1,m-1)
      if usage == nil then usage = { space = true } end
      -- If it's not space, copy usage from the inner room
      if not usage.space then
        usage.inner = true
        hyper.usage.set_usage(walled_room.grid,n,m,usage)
      else
        -- Otherwise check if we're bordering any open squares and therefore need to draw a wall.
        local any_open = false
        for i,normal in ipairs(vector.directions) do
          local near_cell = hyper.usage.get_usage(room.grid,n+normal.x-1,m+normal.y-1)
          if near_cell == nil then near_cell = { space = true } end
          if not near_cell.space and not near_cell.solid then
            any_open = true
            break
          end
        end
        if any_open then
          -- There was at least one open square so we need to make a wall which *could* be carvable.
          -- Note: carvable doesn't mean this can necessarily be used as a door, e.g. Fort crennelations ... that will still happen in room analysis. It
          -- makes the logic overall much simpler.
          -- TODO: Allow diagonal doors here too. Diagonals are problematic (in the previous loop and in analyse_room) because we'd get overlapping anchors
          --       with adjacent walls, this really doesn't sound good.
          local wall_usage = { feature = "rock_wall", wall = true, protect = true }
          for i,normal in ipairs(vector.normals) do
            local near = hyper.usage.get_usage(room.grid,n+normal.x-1,m+normal.y-1)
            if near ~= nil and near.connected then
              wall_usage.carvable = true
              wall_usage.connected = true  -- TODO: Might be redundant
            end
          end
          hyper.usage.set_usage(walled_room.grid, n, m, wall_usage)
        end
      end
    end
  end

  return walled_room
end

function hyper.rooms.add_buffer(room, options)

  local walled_room = {
    type = "transform",
    size = { x = room.size.x + 2, y = room.size.y + 2 },
    generator_used = room.generator_used,
    transform = "add_buffer",
    flags = room.flags,
  }

  -- Take on the existing inner room if available (wall+buffer)
  walled_room.inner_room = room.inner_room or room
  walled_room.inner_room_pos = room.inner_room_pos and { x = room.inner_room_pos.x+1, y = room.inner_room_pos.y+1 } or { x = 1, y = 1 }
  walled_room.grid = hyper.usage.new_usage(walled_room.size.x,walled_room.size.y)

  -- Loop through all the squares and add the buffer
  for m = 0, walled_room.size.y-1, 1 do
    for n = 0, walled_room.size.x-1, 1 do
      -- Get corresponding square from original room
      local usage = hyper.usage.get_usage(room.grid,n-1,m-1)
      if usage == nil then usage = { space = true } end
      -- If it's not space, copy usage from the inner room
      if not usage.space then
        usage.inner = true
        hyper.usage.set_usage(walled_room.grid,n,m,usage)
      else
        -- Otherwise check if we're bordering any non-space and therefore need to create a buffer
        local any_open = false
        for i,normal in ipairs(vector.directions) do
          local near_cell = hyper.usage.get_usage(room.grid,n+normal.x-1,m+normal.y-1)
          if near_cell == nil then near_cell = { space = true } end
          if not near_cell.space then
            any_open = true
            break
          end
        end
        if any_open then
          hyper.usage.set_usage(walled_room.grid, n, m, { space = true, buffer = true })
        end
      end
    end
  end

  return walled_room
end

function hyper.rooms.add_buffer_walls(room, options)
  return hyper.rooms.add_buffer(hyper.rooms.add_walls(room,options),options)
end
