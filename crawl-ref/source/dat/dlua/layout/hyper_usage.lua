------------------------------------------------------------------------------
-- v_usage.lua:
--
-- Usage functions. Manipulates our internal feature grid.
------------------------------------------------------------------------------

hyper.usage = {}

function hyper.usage.new_usage(width, height, initialiser)
  usage_grid = { eligibles = { open = {}, closed = {} }, anchors = {}, width = width, height = height }

  if initialiser == nil then
    initialiser = function()
      return { feature = "space", solid = true, space = true, carvable = true, vault = false, anchors = {} }
    end
  end

  for y = 0, (height-1), 1 do
    usage_grid[y] = { }
    for x = 0, (width-1), 1 do
      usage_grid[y][x] = initialiser(x,y)
    end
  end

  return usage_grid
end

-- Get a square from the usage grid. The value returned is a table which can contain the following values:
--  * vault - indicates this square is part of an existing vault placement and should not generally be overwritten, although
--            it might be connectable to if it's an open square. Might have been placed by an ORIENT primary vault, or
--            could have placed during hyper build by a "tagged" generator.
--  * feature - the feature name at this square. Will be applied to the dungeon grid after build if it's not already applied
--            by a vault. TODO: I was considering converting to glyphs to possibly optimise things, make everything
--            conceptually simpler for people coming from vault design backgrounds, and to support a few new scenarios (e.g.
--            current lack of support for KMONS and a bunch of other mapdef features). But at this stage I don't think it's even feasible (review).
--  * space - Empty space, nothing is applied to this square. Typically from map-defined vaults where there are space
--            characters around the map edges.
--  * open  - Open space where rooms and features can be placed.
--  * anchors - Describes any spots that can be anchored to this cell for connection purposes. (Replacing normal and normal_inverse...)
--  * normal - For eligible walls that can have further rooms attached to them; this indicates the direction in which such a
--            room should be oriented in order to connect to this square.
--  * normal_inverse - Indicates that rooms can be attached in the opposite orientation as well; e.g. a long 1-line-thick wall
--            that can be attached to on either side.
--  * room  - If this square was created as part of a room placement this will be the associated room. (The square could either
--            be part of the room's internal features or part of the walls/doors/etc.) TODO: It might be handy to store both
--            rooms for a partitioning wall, in fact up to four rooms should be possible on a corner.
--  * solid
--  * wall
--  * open_area
function hyper.usage.get_usage(usage_grid,x,y)
  -- Handle out of bounds
  if usage_grid[y] == nil then
    return nil
  end
  if usage_grid[y][x] == nil then
    return nil
  end

  return usage_grid[y][x]
end

function hyper.usage.set_usage(usage_grid,x,y,usage)
  if usage_grid[y] == nil or usage_grid[y][x] == nil then return false end
  -- Check existing usage, remove it from eligibles if it's there
  local current = usage_grid[y][x]
  if current.eligibles_index ~= nil then
    table.remove(usage_grid.eligibles[current.eligibles_which],current.eligibles_index)
  end
  if current ~= usage and current.anchors ~= nil then
    for i,anchor in ipairs(current.anchors) do
      -- TODO: util.remove will be pretty slow on large lists (which is why for eligibles we cache the index)
      --       ...this shouldn't be a huge problem for anchors since there aren't typically tons of them but it's something to watch out for...
      util.remove(usage_grid.anchors,anchor)
    end
  end
  -- Add to the eligibles list if it's eligible. For now never carve vaults (although we could have vaults with a special tag allowing walls to be carved)
  if not usage.vault and (usage.carvable or not usage.solid) then
    usage.spot = { x = x, y = y } -- Store x,y in the usage object otherwise when we look it up in the list we don't know where it came from!
    local which_table = usage.solid and "closed" or "open"
    table.insert(usage_grid.eligibles[which_table],usage)
    usage.eligibles_which = which_table
    usage.eligibles_index = #(usage_grid.eligibles[which_table])  -- Store index of the new item so we can remove it when it's overwritten
  end
  if usage.anchors ~= nil then
    for i,anchor in ipairs(usage.anchors) do
      table.insert(usage_grid.anchors,anchor)
    end
  end
  -- Store usage in grid
  usage_grid[y][x] = usage
end

-- Filter a usage grid
function hyper.usage.filter_usage(usage_grid,filter,transform,region)

  -- Determine region to be filtered
  local x1,y1,x2,y2 = 0,0,usage_grid.width-1,usage_grid.height-1
  if region ~= nil then
    x1,y1,x2,y2 = region.x1,region.y1,region.x2,region.y2
  end

  -- Determine method of filtering if we don't have a callback
  local filter_func = filter
  -- If a filter table is provided then all values have to match
  -- If any more complex filter logic is needed, provide a callback
  if type(filter) == "table" then
    filter_func = function(usage)
                        for k,val in pairs(filter) do
                          if usage[k] ~= val then return false end
                        end
                        return true
                      end
  end

  -- Determine method of transforming the usage when filter matches.
  -- If a transform table is provided then we'll replace each value
  -- from the current usage with the value from transform.
  local transform_func = transform
  if type(transform) == "table" then
    transform_func =  function(usage)
                        -- TODO: Maybe we should take a clone of the usage. Currently it shouldn't be a problem but if the object
                        --       reference is ever duplicated this could alter usage on unexpected squares.
                        for k,val in pairs(transform) do
                          usage[k] = val
                        end
                        return usage
                      end
  end

  for x = x1,x2,1 do
    for y = y1,y2,1 do
      local current = hyper.usage.get_usage(usage_grid,x,y)
      -- Test callback
      if filter_func(current) then
        -- Perform transform
        -- Even though we might just be altering the existing usage object, calling set_usage ensures that
        -- eligibility lists are updated correctly.
        hyper.usage.set_usage(usage_grid, x,y, transform_func(current))
      end
    end
  end
end

function hyper.usage.from_feature_name(feature_name)
  return hyper.usage.from_feature(dgn.feature(feature_num))
end

-- Initialises a usage grid coord by inspecting the
-- existing dungeon grid
function hyper.usage.grid_initialiser(x,y)

  local feature = dgn.grid(x,y)
  local mask = dgn.in_vault(x,y)

  return { feature = feature,
           vault = mask,
           anchors = {},
           space = false,
           carvable = false,
           solid = not (feat.has_solid_floor(feature) or feat.is_door(feature)),
           wall = feat.is_wall(feature)
         }

end

-- Scan a grid and determine potential usage of features.
-- Features on the edge of the map can be flagged for external connection
-- - this allows this grid to be joined onto existing features on the map.
-- Features inside the map can be flagged for internal connection
-- - e.g. placing rooms within rooms, or carving into a rock area inside the room.
function hyper.usage.analyse_grid_usage(usage_grid,options)

  local get = hyper.usage.get_usage
  local set = hyper.usage.set_usage

  -- TODO: An optimised zonify would do this job pretty well and
  -- possibly get more useful information.

  for x = 0, usage_grid.width-1, 1 do
    for y = 0, usage_grid.height-1, 1 do
      local usage = get(usage_grid,x,y)
      -- Ignore vaults
      if not usage.vault then
        -- Check walls for carvability
        if usage.wall then
          for i,normal in ipairs(vector.normals) do
            local near = get(usage_grid,x+normal.x,y+normal.y)
            if near ~= nil and not near.solid then
              usage.carvable = true
              table.insert(usage.anchors,{ normal = vector.normals[(normal.dir+2)%4 + 1], pos = { x=0,y=0 }, grid_pos = { x=x,y=y }})
            end
          end
        -- Check open spaces for bordering walls
        elseif not usage.solid then
          local no_wall = true
          for i,normal in ipairs(vector.directions) do
            local near = hyper.usage.get_usage(usage_grid,x+normal.x,y+normal.y)
            if near ~= nil and near.solid then usage.buffer = true end
          end
        end
        -- Reapply the usage - this will update the
        -- global anchors list
        set(usage_grid,x,y,usage)
      end
    end
  end
end
