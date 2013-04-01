------------------------------------------------------------------------------
-- hyper_strategy.lua:
--
-- Defines different strategies for placing rooms (well, chunks of map) onto
-- the layout, using a system of various different callbacks for different
-- parts of the process.
--
-- By dividing things up in this way it's easier to write the logic for how different
-- types of rooms are placed and how they veto, and to reuse chunks of the
-- logic in different contexts. There are several available callbacks:
--
--   * pick_room    -- picks a room to attach to or place within
--   * pick_place   -- picks the coordinate on that room where we will place
--   * pick_anchor  -- picks an anchor and orientation (usually an edge cell) for the room that is being placed
--   * veto_place   -- allows the position to be vetoed before cells are inspected
--   * veto_cell    -- called as each cell is inspected to see what grid cells the map cells are overlapping
--   * veto_after   -- called after all cells are inspected; we can collate data during cell
--                     inspection and act at the end, e.g. "this shouldn't overlap more than two rooms"
--   * apply_cell   -- actually applies the cell to the usage grid
--   * post_place   -- called after placement to perform any post-processing
--   * sort_room    -- handles sorting the room into pick tables after placement

-- TODO: Allow a flow where the place is picked first *then* the room? This works best if we *definitely* know
--       that we're going to be able to place the room there, since room generation is a slow operation.
-- TODO: Actually support all the callbacks

------------------------------------------------------------------------------
-- Place picking callbacks
--

-- Pick a random coord from the whole grid; ignore usage. Mainly useful for placing a
-- primary room but could have some edge cases like slapping decor onto a layout or
-- making crazy abyss layouts, or a layout where square rooms all overlap each other
-- as long as we can make connectivity work...
function hyper.place.pick_place_random(room,build,usage_grid,options)

  -- TODO: If rotation is allowed, decide rotation first so we know how much padding to leave

  -- Work out our boundaries
  local gxm,gym = dgn.max_bounds()
  local bounds = room.generator_used.bounds or { x1 = 0, y1 = 0, x2 = usage_grid.width, y2 = usage_grid.height}
  local padding = room.generator_used.place_padding or 0
  local xmin,xmax,ymin,ymax = bounds.x1+padding, bounds.x2-1-room.size.x-padding, bounds.y1+padding, bounds.y2-1-room.size.y-padding

  local x,y = crawl.random_range(xmin,xmax),crawl.random_range(ymin,ymax)
  return { pos = { x = x, y = y }, usage = hyper.usage.get_usage(usage_grid,x,y) }
end

-- Pick an open spot from the list of eligible floors in usage_grid.
function hyper.place.pick_place_open(room,build,usage_grid,options)
  -- Select an eligible spot from the list in usage_grid
  local available_spots = #(usage_grid.eligibles.open)
  if available_spots == 0 then return hyper.place.pick_place_random(room,build,usage_grid,options) end

  local usage = util.random_from(usage_grid.eligibles.open)
  return { pos = usage.spot, usage = usage }

  -- TODO: Following shouldn't be necessary any more since we can intentionally find a room to attach to rather
  -- than being completely random about it ...

  -- Scan a 5x5 grid around an open spot, see if we find an eligible_open wall to attach to. This makes us much more likely to
  -- join up rooms in open areas, and reduces the amount of 1-tile-width chokepoints between rooms. It's still fairly simplistic
  -- though and maybe we could do this around the whole room area later on instead?
  --local near_eligibles = {}
  --for p in iter.rect_iterator(dgn.point(spot.x-2,spot.y-2),dgn.point(spot.x+2,spot.y+2)) do
  --  local near_usage = hyper.usage.get_usage(usage_grid,p.x,p.y)
  --  if near_usage ~= nil and near_usage.carvable and near_usage.room ~= usage.room then  -- We check it's not from the same room so we don't stop placing rooms within rooms
    --  table.insert(near_eligibles, { spot = p, usage = near_usage })
  --  end
  --end

  -- Randomly pick one of the new spots; maybe_place_vault will at least attempt to attach the room here instead, if possible
  --if #near_eligibles > 0 then
  --  local picked = near_eligibles[crawl.random2(#near_eligibles)+1]
  --  spot = picked.spot
  --  usage = picked.usage
  --end
end

function hyper.place.pick_place_closed(room,build,usage_grid,options)

  local available_spots = #(usage_grid.eligibles.closed)
  if available_spots == 0 then return hyper.place.pick_place_random(room,build,usage_grid,options) end

  local usage = util.random_from(usage_grid.eligibles.open)
  return { pos = usage.spot, usage = usage }
end

------------------------------------------------------------------------------
-- Anchor picking callbacks
--

-- Anchors the room by the top-left corner, no rotation
function hyper.place.anchor_origin()
  return { dir = 0, origin = { x = 0, y = 0 } }
end

-- Rotates the room to a random direction
-- TODO: Pick a random edge square for the origin instead of the corner
function hyper.place.anchor_random()
  return { dir = crawl.random2(4), origin = { x = 0, y = 0 } }
end

-- Anchor to a random (connectable) wall
function hyper.place.anchor_wall(place,room,build,usage_grid,options)
  if #(room.grid.anchors)==0 then return false end

  local anchor = util.random_from(room.grid.anchors)

  if not place.usage.solid or #(place.usage.anchors) == 0 then
    -- Pick a random rotation for the door wall
    v_normal_dir = util.random_from(vector.normals).dir
  else
    -- If placing a room in an existing wall we have the normal stored already in the usage data
    local anchor = util.random_from(place.usage.anchors)
    v_normal_dir = anchor.normal.dir
  end

  local final_orient = (v_normal_dir - anchor.normal.dir + 2) % 4

  return { origin = { x = anchor.origin.x + anchor.pos.x, y = anchor.origin.y + anchor.pos.y }, dir = final_orient }
end

-- Picks a connectable wall spot to anchor to
-- TODO: This is the version that picked a wall then an anchor. It was nice because it gave
-- each potential orientation an equal chance on e.g. long thin rooms. Right now however we
-- don't group the anchors by wall but depending on how things turn out it might be good to reinstate this.
function hyper.place.anchor_wall_old(place,room,build,usage_grid,options)
  -- Choose which wall of the room to attach to this spot (walls were determined in room analysis)
  local orients = { }
  for n = 0, 3, 1 do
    if room.walls[n].eligible then
      table.insert(orients,{ dir = n, wall = room.walls[n] })
    end
  end
  local count = #orients

  if count == 0 then return false end

  -- Now select which of the room's connectable squares we're going to use to attach to the eligible cell
  local v_normal, v_wall, v_normal_dir, wall

  local chosen = orients[crawl.random2(count) + 1]
  wall = chosen.wall
  -- Pick a random open cell on the wall. This is where we'll attach the room to the door.
  if #wall == 0 then return false end -- Shouldn't happen, eligible should mean cells available
  local chosen_wall = wall[crawl.random2(#wall)+1]
  local pos = chosen_wall.pos
  -- Get wall's normal and calculate the door vector (i.e. attaching wall)
  -- In open space we can pick a random normal for the door
  if not usage.solid and #(usage.anchors) == 0 then
    -- Pick a random rotation for the door wall
    v_normal = vector.normals[crawl.random_range(1,4)]
    v_normal_dir = v_normal.dir

  -- If placing a room in an existing wall we have the normal stored already in the usage data
  else
    -- Pick a random anchor
    local anchor = usage.anchors[crawl.random2(#(usage.anchors))+1]
    spot = anchor.grid_pos
    v_normal = anchor.normal
    v_normal_dir = v_normal.dir
  end

  -- Calculate how much the room will have to rotate to match the new orientation
  local final_orient = (v_normal_dir - chosen_wall.normal.dir + 2) % 4
  return { dir = final_orient, origin = spot }

end

------------------------------------------------------------------------------
-- Cell veto callbacks
--

-- Standard check that most or all vetos should observe. Prevents a
-- room placing off the map edge or overlapping a vault. If ignoring these
-- check, special handling should be in place elsewhere so you don't for
-- instance draw floor tiles at the map border. (TODO: Actually we prevent
-- overwriting vaults in any fashion, maybe should also automatically
-- prevent map border floor)
function hyper.place.cell_veto_standard(coord,state)
  local target_usage,usage = coord.grid_usage,state.usage

  -- Should handle these nil values in the loop instead?
  if target_usage == nil and coord.room_cell.buffer then return true end
  if target_usage == nil then
    if coord.room_cell.space then return false else return true end
  end

  -- Don't ever overwrite vaults
  if target_usage.vault then return true end

  return false
end


-- Forces placement (dangerous!)
function hyper.place.cell_veto_none(coord,state)
  return false
end

-- Default check - allows placement in open area, carving into rock,
-- rooms within rooms, and attaching rooms onto to each other.
function hyper.place.cell_veto_normal(coord,state)
  if hyper.place.cell_veto_standard(coord,state) then return true end

  local target_usage,usage = coord.grid_usage,state.usage
  if target_usage == nil then return false end

  -- TODO: We can do interesting stuff by allowing rooms to overlap a bit
  -- Open placement
  if not usage.solid then
    -- Only paint on the same room
    if (target_usage.room ~= usage.room)
      -- Avoid walls
      or (coord.room_cell.buffer and (target_usage.solid or target_usage.wall)) then return true end
  -- Open attached placement
  elseif usage.open_area then
    -- Prevent overlap with internal floor of room we're attaching to
    if not coord.room_cell.space and (target_usage.room == usage.room) and not target_usage.wall then return true end
    -- Prevent overlap with incidental rooms
    if (target_usage.room ~= usage.room) and not coord.room_cell.space and not coord.room_cell.buffer and (target_usage.solid or target_usage.restricted) then return true end
  else
    -- Enclosed placement, we can only burrow into more rock, or overlap our walls with incidental walls, or floor edges with carvable space
    if not coord.room_cell.space and not target_usage.solid then return true end
  end
  return false

end

function hyper.place.cell_veto_open(coord,state)
  if hyper.place.cell_veto_standard(coord,state) then return true end

  local target_usage,usage = coord.grid_usage,state.usage
  if target_usage == nil then return false end

  -- Open placement
  if not usage.solid then
    -- Only paint on the same room
    if (target_usage.room ~= usage.room)
      -- Avoid walls
      or (coord.room_cell.buffer and (target_usage.solid or target_usage.wall)) then return true end
  -- Open placement anchored to an existing room
  else
    -- Prevent overlap with internal floor of room we're attaching to
    if not coord.room_cell.space and (target_usage.room == usage.room) and not target_usage.wall then return true end
    -- Prevent overlap with incidental rooms
    if (target_usage.room ~= usage.room) and not coord.room_cell.space and not coord.room_cell.buffer and (target_usage.solid or target_usage.restricted) then return true end
  end
  return false

end

------------------------------------------------------------------------------
-- Placement strategies
--
-- Some very common groupings of callbacks. Some build passes might want to
-- compose custom strategies instead based on these.

-- For placing an initial large room typically encompassing the level.
-- We don't veto because we may want to underlay an existing orient vault on the level.
-- TODO: For odd shaped layouts (e.g. large cross) the orient vault isn't guaranteed to get connected.
-- There are a number of ways we could enforce a connection; the most obvious is checking at the end
-- of build to see whether it connected and if not use e.g. spotty layout connector to join it up
-- to the nearest available external anchor.
hyper.place.strategy_primary = {
  pick_place = hyper.place.pick_place_random,
  pick_anchor = hyper.place.anchor_origin,
  veto_cell = hyper.place.cell_veto_none
}

-- Places rooms randomly within a large open area.
-- TODO: Doesn't work properly
hyper.place.strategy_random = {
  pick_place = hyper.place.pick_place_random,
  pick_anchor = hyper.place.anchor_random,
  veto_cell = hyper.place.cell_veto_standard
}

-- Places in open areas using internal/open anchors only.
hyper.place.strategy_open = {
  pick_place = hyper.place.pick_place_open,
  pick_anchor = hyper.place.anchor_wall,
  veto_cell = hyper.place.cell_veto_open
}

-- Places rooms only inside other rooms
-- Looks for class="{inner_class}" on the room's generators
hyper.place.strategy_inner = {
  pick_place = hyper.place.pick_place_open,
  pick_anchor = hyper.place.anchor_wall,
  veto_place = function(state,usage_grid)
    if state.usage.room == nil or not state.usage.room.class == state.room.generator_used.inner_class then return true end
  end,
  veto_cell = hyper.place.cell_veto_open
}

-- Carves into rock using only external anchors
hyper.place.strategy_carve = {
  pick_place = hyper.place.pick_place_closed,
  pick_anchor = hyper.place.anchor_wall,
  veto_place = function(state,usage_grid)

  end,
  veto_cell = function(coord,state)  --hyper.place.cell_veto_normal
    if hyper.place.standard_veto(coord,state) then return true end
    -- Enclosed placement, we can only burrow into more rock, or overlap our walls with incidental walls, or floor edges with carvable space
    if not coord.room_cell.space and not coord.grid_usage.solid and not coord.grid_usage.restricted then return true end
  end
}

hyper.place.strategy_inner_or_carve = {
  pick_place = function(room,build,usage_grid,options)
    if crawl.coinflip() then
      return hyper.place.strategy_inner.pick_place(room,build,usage_grid,options)
    else
      return hyper.place.strategy_carve.pick_place(room,build,usage_grid,options)
    end
  end,
  veto_place = function(state,usage_grid)
    if state.usage.open then
      return hyper.place.strategy_inner.veto_place(state,usage_grid)
    else
      return hyper.place.strategy_carve.veto_place(state,usage_grid)
    end
  end,
  pick_anchor = function(place,room,build,usage_grid,options)
    if place.usage.open then
      return hyper.place.strategy_inner.pick_anchor(room,build,usage_grid,options)
    else
      return hyper.place.strategy_carve.pick_anchor(room,build,usage_grid,options)
    end
  end,
  veto_cell = function(coord,state)
    if hyper.place.standard_veto(coord,state) then return true end

    if state.usage.open then
      return hyper.place.strategy_inner.veto_cell(coord,state)
    else
      return hyper.place.strategy_carve.veto_cell(coord,state)
    end
  end
}
