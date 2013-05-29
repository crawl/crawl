------------------------------------------------------------------------------
-- v_decor.lua:
--
-- Functions that handle decorating walls, doors, and other points of interest
------------------------------------------------------------------------------

hyper.decor = {}

------------------------------------------------------------------------------
-- Decorator functions
--
-- These callbacks decorate walls specifically where rooms join together.
-- They mainly paint doors and windows, but sometimes open space.

-- Paints all connecting points with floor to make completely open rooms
function hyper.rooms.decorate_walls_open(state, connections, door_required)

  for i, door in ipairs(connections) do
    dgn.grid(door.grid_pos.x,door.grid_pos.y, "floor")
  end

end

-- Opens walls randomly according to a chance which can be modified by setting open_wall_chance on the generator
function hyper.rooms.decorate_walls_partially_open(state, connections, door_required)

  -- Start by opening a single wall if we definitely need one
  if door_required then
    door = connections[crawl.random2(#connections)+1]
  end
  local door_chance = state.room.generator_used.open_wall_chance or 50
  for i, door in ipairs(connections) do
    if crawl.x_chance_in_y(door_chance,100) then
      dgn.grid(door.grid_pos.x,door.grid_pos.y, "floor")
    end
  end

end

function hyper.rooms.decorate_walls(state, connections, door_required, has_windows, floors_instead)

  -- TODO: There is a slight edge case that can produce broken layouts. Where a room manages to place
  --       in such a way that the origin wall intersects with a second room, we might end up placing doors
  --       on that room instead and not the origin. Technically even this shouldn't matter because the
  --       second room should already be correctly connected to everything else, but I'm sure there is some
  --       situation where this could go wrong. What we should really do is divide the walls up by which
  --       room they attach to, rather than their normal; this means we can force door_required for
  --       definite on the origin wall. It will also make it easy to e.g. pick and choose how we decorate
  --       based on which room we're attaching to, rather than having to iterate through all the connections,
  --       and it solves the problem in Dis layouts where we wanted to choose a decorator for the whole
  --       outside room, not per wall.
  local have_door = true
  if not door_required then
    have_door = crawl.x_chance_in_y(5,6)
  end

  if have_door then
    -- TODO: Following was initially for Snake bubbles. But perhaps in V we could occasionally use floor or open_door.
    local door_feature = "closed_door"
    if floors_instead == true then door_feature = "floor" end
    -- TODO: Raise or lower the possible number of doors depending on how many connections we actually have
    local num_doors = math.abs(crawl.random2avg(9,4)-4)+1  -- Should tend towards 1 but rarely can be up to 5
    for n=1,num_doors,1 do
      -- This could pick a door we've already picked; doesn't matter, this just makes more doors even slightly
      -- less likely
      local door = connections[crawl.random2(#connections)+1]
      if door ~= nil then
        dgn.grid(door.grid_pos.x,door.grid_pos.y, door_feature)
        door.is_door = true
      end
    end
  end

  -- Optionally place windows
  if has_windows == false or state.room.no_windows
     or state.options.no_windows or not crawl.one_chance_in(5) then return end

  -- Choose feature for window
  -- TODO: Pick other features in way that's similarly overrideable at multiple levels
  local window_feature = "clear_stone_wall"
  if state.options.layout_window_type ~= nil then window_feature = state.options.layout_window_type end
  if state.room.generator_used.window_type ~= nil then window_feature = state.room.generator_used.window_type end
  if state.window_type ~= nil then window_feature = state.window_type end

  local num_windows = math.abs(crawl.random2avg(5,3)-2)+2  -- Should tend towards 2 but rarely can be up to 4

  for n=1,num_windows,1 do
    -- This could pick a door we've already picked; doesn't matter, this just makes more doors even slightly less likelier
    local door = connections[crawl.random2(#connections)+1]
    if door ~= nil and not door.is_door then
      dgn.grid(door.grid_pos.x,door.grid_pos.y, window_feature)
      door.is_window = true
    end
  end
end

------------------------------------------------------------------------------
-- Decor rooms
--
-- These rooms are designed to be placed inside other rooms to decorate them,
-- sometimes in open space.

-- function hyper.decor.phase_pattern(room,options,gen)
  -- local pattern = "x."
-- end
