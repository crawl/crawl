------------------------------------------------------------------------------
-- hyper.lua: hyper^3 layout engine
--
-- Main include for Hyper layouts. Sets up namespaces, defines some useful arrays and common functions.
------------------------------------------------------------------------------

hyper = {}          -- Main namespace for engine

-- This switch dumps helpful diagrams of rooms and layouts out to console.
-- Only advisable if you start tiles from a command-line.
hyper.debug = false
-- Outputs some profiling information on the layout build
hyper.profile = false

crawl_require("dlua/layout/vector.lua")
crawl_require("dlua/layout/hyper_usage.lua")
crawl_require("dlua/layout/hyper_paint.lua")
crawl_require("dlua/layout/hyper_place.lua")
crawl_require("dlua/layout/hyper_strategy.lua")
crawl_require("dlua/layout/hyper_rooms.lua")
crawl_require("dlua/layout/hyper_shapes.lua")
crawl_require("dlua/layout/hyper_decor.lua")
crawl_require("dlua/layout/rooms_primitive.lua")
crawl_require("dlua/layout/hyper_debug.lua")
crawl_require("dlua/profiler.lua")

-- The four directions and their associated vector normal and name.
-- This helps us manage orientation of rooms.
vector.normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

-- Sometimes it's useful to have a list of diagonal vectors also
vector.diagonals = {
  { x = -1, y = -1, dir = 0.5, name="nw" },
  { x = -1, y = 1, dir = 1.5, name="sw" },
  { x = 1, y = 1, dir = 2.5, name="se" },
  { x = 1, y = -1, dir = 3.5, name="ne" }
}

-- All 8 directions (although not in logical order)
vector.directions = util.append({},vector.normals,vector.diagonals)

-- Default options table
function hyper.default_options()

  local options = {
    max_rooms = 27, -- Maximum number of rooms to attempt to place
    max_room_depth = 0, -- No max depth (not implemented yet anyway)
    min_distance_from_wall = 2, -- Room must be at least this far from outer walls (in open areas). Reduces chokepoints.
    -- The following settings (in addition to max_rooms) can adjust how long it takes to build a level, at the expense of potentially less intereting layouts
    max_room_tries = 27, -- How many *consecutive* rooms can fail to place before we exit the entire routine
    max_place_tries = 50, -- How many times we will attempt to place *each room* before picking another
    strict_veto = true, -- Layouts will do everything they can do avoid isolated areas

    -- Weightings of various types of room generators. The plan is to better support code vaults here.
    room_type_weights = {
      { generator = "code", paint_callback = rooms.primitive.floor, weight = 1, min_size = 4, max_size = 40, empty = true }, -- Floor vault
    },

    -- Rock seems a sensible default
    layout_wall_weights = {
      { feature = "rock_wall", weight = 1 },
    },
    layout_floor_type = "floor"  -- Should probably never need to be anything else

  }

  return options

end

-- Main layout build function. Options is a table containing the global options and build fixture
-- required to build the entire layout. TODO: Since this is basically the primary API for the hyper
-- engine, need to properly document the options table separately.
function hyper.build_layout(e, options)
  if not options.test and e.is_validating() then return true end

  name = options.name or "Hyper"

  if hyper.profile then
    profiler.start(name)
    profiler.push("InitOptions")
  end

  if hyper.debug then print("Hyper Layout: " .. name) end

  local default_options = hyper.default_options()
  if options ~= nil then options = hyper.merge_options(default_options,options) end

  -- Map legacy global options to a single build fixture, rather than edit each layout right now
  if options.build_fixture == nil then
    options.build_fixture = { { type = "build", generators = options.room_type_weights } }
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("InitUsage")
  end

  local gxm,gym = dgn.max_bounds()
  local main_state = {
    usage_grid = hyper.usage.new_usage(gxm,gym,
                                       hyper.usage.grid_initialiser),
    results = {}
  }

  -- Make a helper function to make it easier to get/set usage
  function main_state.usage(x,y,set)
    if set ~= nil then
      return hyper.usage.set_usage(main_state.usage_grid,x,y,set)
    else
      return hyper.usage.get_usage(main_state.usage_grid,x,y)
    end
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("ScanUsage")
  end

  -- Scan the existing dungeon grid and update the usage grid accordingly
  -- TODO: Would be a performance boost not running this if we're the
  -- primary layout anyway and just assume no existing anchors etc.
  hyper.usage.analyse_grid_usage(main_state.usage_grid,options)

  if hyper.profile then
    profiler.pop()
    profiler.push("BuildFixture")
  end

  -- Perform each task in the build fixture
  for i, item in ipairs(options.build_fixture) do
    if item.enabled == nil
      or type(item.enabled) == "function" and item.enabled(item,main_state)
      or item.enabled == true then

      if hyper.profile then
        profiler.push("BuildPass", { pass = item.pass })
      end

      -- Applies a paint table to the whole layout
      -- Here for legacy purposes, generally use "place" or just "build" to setup initial layout
      -- TODO: Eventually convert old layouts to use build/place large rooms and remove this
      if item.type == "paint" then
        -- Easy way to do this is wrap the paint array into a room function the size of the map and force placement at 0,0...

      -- Perform a normal build operation, picking and placing multiple rooms/vaults/shapes from a weighted list
      elseif item.type == "build" then
        local results = hyper.place.build_rooms(item,main_state.usage_grid,options)
        -- Append results to global list (TODO: slightly broken for V because it won't update stairs...)
        util.append(main_state.results,results)

      -- Places a single room or feature, usually at a specified position; just shortcuts some of the fluff you'd
      -- need to do the same in "build"
      elseif item.type == "place" then
        -- TODO:
      -- Perform a filter/transform operation on the whole usage/feature grid or a region of it
      elseif item.type == "filter" then
        hyper.usage.filter_usage(main_state.usage_grid, item.filter, item.transform, item.region)
      end

      if hyper.profile then
        profiler.pop()
      end

    end

  end

  -- Updates the dungeon grid
  -- TODO: Right now it happens room-by-room so I've commented this out.
  -- But there should be a performance gain in only applying everything right at the end.
  -- hyper.usage.apply_usage(usage_grid)

  if options.post_fixture_callback ~= nil then
    if hyper.profile then
      profiler.pop()
      profiler.push("PostFixture")
    end

    options.post_fixture_callback(main_state,options)
  end

  if hyper.profile then
    profiler.pop()
    profiler.push("VaultMask")
  end

  -- Set MMT_VAULT across the whole map depending on usage. This prevents the dungeon builder
  -- placing standard vaults in places where it'll mess up our architecture.
  local gxm, gym = dgn.max_bounds()
  for p in iter.rect_iterator(dgn.point(0, 0), dgn.point(gxm - 1, gym - 1)) do
    local usage = hyper.usage.get_usage(usage_grid,p.x,p.y)
    if usage ~= nil and usage.protect then
      dgn.set_map_mask(p.x,p.y)
    end
  end

  if hyper.profile then
    profiler.pop()
    profiler.stop()
  end

  return main_state
end

-- Merges together two options tables (usually the default options getting
-- overwritten by a minimal set provided by an individual layout)
function hyper.merge_options(base,to_merge)
  if to_merge == nil or type(to_merge) ~= "table" then
    if hyper.debug then
      print("Error: to_merge not a table: " .. type(to_merge) .. ": " .. to_merge)
      crawl.dump_stack()
    end
    return
  end
  -- Quite simplistic, right now this won't recurse into sub tables (it might need to)
  for key, val in pairs(to_merge) do
    base[key] = val
  end
  return base
end
