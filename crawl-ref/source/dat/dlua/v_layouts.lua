------------------------------------------------------------------------------
-- v_layouts.lua:
--
-- Main layouts include for Vaults.
--
-- Code by mumra
-- Based on work by infiniplex, based on original designs and code by mumra.
--
-- Contains the functions that actually build the layouts.
--
-- Notes:
-- The original had the entire layout code in layout_vaults.des under a
-- single map def. This made things very hard to understand. Here I am
-- separating things into some smaller separate which can be called
-- from short map headers in layout_vaults.des.
--
-- The layout functions set up an array of floor/wall areas then call the paint
-- functions and room building functions. It'd be easy to allow more complex
-- geometries by passing in callbacks or enhancing the paint function but in
-- general we only want wide straight corridors or open rectangular areas so
-- room/door placement can work correctly.
------------------------------------------------------------------------------

hypervaults = {}  -- Old namespace, gradually moving everything into hyper.*

crawl_require("dlua/v_debug.lua")
crawl_require("dlua/v_paint.lua")
crawl_require("dlua/v_rooms.lua")
crawl_require("dlua/v_shapes.lua")
crawl_require("dlua/ghost.lua")

_VAULTS_DEBUG = false

-- The four directions and their associated vector normal and name.
-- This helps us manage orientation of rooms.
hypervaults.normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

-- Default parameters for all Vaults layouts. Some individual layouts might tweak these parameters to create specific effects.
function vaults_default_options()

  -- Decide weights for wall types based on depth
  local depth = you.depth()
  local stone_weight = (4-depth) * 10              -- 30,20,10,0
  local metal_weight = math.max(0,(depth-2) * 20)  -- 0,0,20,40
  local crystal_weight = math.max(0,depth-2)       -- 0,0,1,2

  -- Main options table
  local options = {
    max_rooms = 27, -- Maximum number of rooms to attempt to place
    max_room_depth = 0, -- No max depth (not implemented yet anyway)
    min_distance_from_wall = 2, -- Room must be at least this far from outer walls (in open areas). Reduces chokepoints.
    -- The following settings (in addition to max_rooms) can adjust how long it takes to build a level, at the expense of potentially less intereting layouts
    max_room_tries = 20, -- How many *consecutive* rooms can fail to place before we exit the entire routine
    max_place_tries = 50, -- How many times we will attempt to place *each room* before picking another

    -- Special option for Vaults (handled in hyper.vaults.floor_vault) - chance out of 100 of placing a stair somewhere in the middle of the floor vault
    stair_chance = 80,

    -- Weightings of various types of room generators.
    room_type_weights = {
      { generator = "code", paint_callback = hypervaults.shapes.floor_vault, weight = 80, min_size = 6, max_size = 15 }, -- Floor vault
      { generator = "tagged", tag = "vaults_room", weight = 50, max_rooms = 6 },
      { generator = "tagged", tag = "vaults_empty", weight = 40 },
      { generator = "tagged", tag = "vaults_hard", weight = 10, max_rooms = 1 },
      { generator = "tagged", tag = "vaults_entry_crypt", weight = (you.where() == dgn.level_name(dgn.br_entrance("Crypt"))) and 25 or 0, max_rooms = 1 },
      -- Create a tagged generator to represent ghost vaults. The weight should
      -- be 0 since this generator is only chosen through a specific ghost
      -- chance roll.
      { generator = "tagged", tag = "vaults_ghost", weight = 0, max_rooms = 1 },
      -- Remove the Forest and Blade entries when TAG_MAJOR_VERSION > 34
      { generator = "tagged", tag = "vaults_entry_forest", weight = (you.where() == dgn.level_name(dgn.br_entrance("Forest"))) and 25 or 0, max_rooms = 1 },
      { generator = "tagged", tag = "vaults_entry_blade", weight = (you.where() == dgn.level_name(dgn.br_entrance("Blade"))) and 25 or 0, max_rooms = 1 },
    },

    -- Weightings for types of wall to use across the whole layout
    layout_wall_weights = {
      { feature = "stone_wall", weight = stone_weight },
      { feature = "metal_wall", weight = metal_weight },
      { feature = "crystal_wall", weight = crystal_weight },
    },
    layout_floor_type = "floor"

  }

  return options
end

-- Default options table
function hypervaults.default_options()

  local options = {
    max_rooms = 27, -- Maximum number of rooms to attempt to place
    max_room_depth = 0, -- No max depth (not implemented yet anyway)
    min_distance_from_wall = 2, -- Room must be at least this far from outer walls (in open areas). Reduces chokepoints.
    -- The following settings (in addition to max_rooms) can adjust how long it takes to build a level, at the expense of potentially less intereting layouts
    max_room_tries = 27, -- How many *consecutive* rooms can fail to place before we exit the entire routine
    max_place_tries = 50, -- How many times we will attempt to place *each room* before picking another

    -- Weightings of various types of room generators. The plan is to better support code vaults here.
    room_type_weights = {
      { generator = "code", paint_callback = floor_vault_paint_callback, weight = 1, min_size = 4, max_size = 40, empty = true }, -- Floor vault
    },

    -- Rock seems a sensible default
    layout_wall_weights = {
      { feature = "rock_wall", weight = 1 },
    },
    layout_floor_type = "floor"  -- Should probably never need to be anything else

  }

  return options

end

-- TODO: The 'paint' parameter should disappear. Instead the options will contain a setup array. This could contain paint instructions,
-- but alternately should be able to wire room generators to create initial terrain. Since rooms can now be created from paint arrays _anyway_,
-- it seems that pre-painting the layout is completely unnecessary and room generation can do anything ...
function hypervaults.build_layout(e, name, paint, options)
  if e.is_validating() then return; end

  if _VAULTS_DEBUG then print("Hypervaults Layout: " .. name) end

  local default_options = hypervaults.default_options()
  if options ~= null then hypervaults.merge_options(default_options,options) end

  local data = paint_vaults_layout(paint, default_options)
  local rooms = place_vaults_rooms(e, data, default_options.max_rooms, default_options)
end

-- Merges together two options tables (usually the default options getting
-- overwritten by a minimal set provided by an individual layout)
function hypervaults.merge_options(base,to_merge)
  -- Quite simplistic, right now this won't recurse into sub tables (it might need to)
  for key, val in pairs(to_merge) do
    base[key] = val
  end
  return base
end

-- Build any vaults layout from a paint array, useful to quickly prototype
-- layouts in layout_vaults.des
function build_vaults_layout(e, name, paint, options)
  if e.is_validating() then return; end

  if _VAULTS_DEBUG then print("Vaults Layout: " .. name) end

  local defaults = vaults_default_options()
  if options ~= nil then hypervaults.merge_options(defaults,options) end

  local data = paint_vaults_layout(paint, defaults)
  local rooms = place_vaults_rooms(e, data, defaults.max_rooms, defaults)
end

function build_vaults_ring_layout(e, corridorWidth, outerPadding)

  local gxm, gym = dgn.max_bounds()

  local c1 = { x = outerPadding, y = outerPadding }
  local c2 = { x = outerPadding + corridorWidth, y = outerPadding + corridorWidth }
  local c3 = { x = gxm - outerPadding - corridorWidth - 1, y = gym - outerPadding - corridorWidth - 1 }
  local c4 = { x = gxm - outerPadding - 1, y = gym - outerPadding - 1 }

  -- Paint four floors
  local paint = {
    { type = "floor", corner1 = c1, corner2 = c4},
    { type = "wall", corner1 = c2, corner2 = c3 }
  }

  build_vaults_layout(e, "Ring", paint, { max_room_depth = 3 })

end

function build_vaults_cross_layout(e, corridorWidth, intersect)

  -- Ignoring intersect for now
  local gxm, gym = dgn.max_bounds()

  local corridorWidth = 3 + crawl.random2avg(3,2)

  local xc = math.floor((gxm-corridorWidth)/2)
  local yc = math.floor((gym-corridorWidth)/2)
  local paint = {
    { type = "floor", corner1 = { x = xc, y = 1 }, corner2 = { x = xc + corridorWidth - 1, y = gym - 2 } },
    { type = "floor", corner1 = { x = 1, y = yc }, corner2 = { x = gxm - 2, y = yc + corridorWidth - 1 } }
  }

  build_vaults_layout(e, "Cross", paint, { max_room_depth = 4 })

end

function build_vaults_big_room_layout(e, minPadding,maxPadding)
  -- The Big Room
  local gxm, gym = dgn.max_bounds()
  local padx,pady = crawl.random_range(minPadding,maxPadding),crawl.random_range(minPadding,maxPadding)

  -- Will have a ring of outer rooms but the central area will be chaotic city
  local paint = {
    { type = "floor", corner1 = { x = padx, y = pady }, corner2 = { x = gxm - padx - 1, y = gym - pady - 1 } }
  }
  build_vaults_layout(e, "Big Room", paint, { max_room_depth = 4, max_rooms = 23, max_room_tries = 12, max_place_tries = 30 })

end

function build_vaults_chaotic_city_layout(e)
  local gxm, gym = dgn.max_bounds()

  -- Paint entire level with floor
  local paint = {
    { type = "floor", corner1 = { x = 1, y = 1 }, corner2 = { x = gxm - 2, y = gym - 2 } }
  }

  build_vaults_layout(e, "Chaotic City", paint, { max_room_depth = 5, max_rooms = 25, max_room_tries = 10, max_place_tries = 20 })

end

function build_vaults_maze_layout(e,veto_callback, name)
  local gxm, gym = dgn.max_bounds()
  if name == nil then name = "Maze" end

  -- Put a single empty room somewhere roughly central. All rooms will be built off from each other following this
  local x1 = crawl.random_range(30, gxm-70)
  local y1 = crawl.random_range(30, gym-70)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + crawl.random_range(4,10), y = y1 + crawl.random_range(4,10) } }
  }

  build_vaults_layout(e, name, paint, { max_room_depth = 0, veto_place_callback = veto_callback })

end
-- Uses a custom veto function to alternate between up or down connections every {dist} rooms
function build_vaults_maze_snakey_layout(e)

  local which = 0
  if crawl.coinflip() then which = 1 end

  -- How many rooms to draw before alternating
  -- When it's 1 it doesn't create an obvious visual effect but hopefully in gameplay
  -- it will be more obvious.
  -- TODO: This range can be higher if the callback has the coords (which it now does on state.pos or state.base),
  --       if we're near the edge then bailout.
  local dist = crawl.random_range(1,2)
  -- Just to be fair we'll offset by a number, this will determine how far from the
  -- first room it goes before the first switch
  local offset = crawl.random_range(0,dist-1)
  -- Callback function
  local function callback(state)
    if state.usage.normal == nil or state.usage.depth == nil then return false end
    local odd = (math.floor(state.usage.depth/dist)+offset) % 2
    if odd == which then
      if state.usage.normal.y == 0 then return true end
      return false
    end
    if state.usage.normal.x == 0 then return true end
    return false
  end
  build_vaults_maze_layout(e, callback, "Snakey Maze")
end

-- Goes just either horizontally or vertically for a few rooms then branches out, makes
-- an interesting sprawly maze with two bulby areas
function build_vaults_maze_bifur_layout(e)

  local which = crawl.coinflip()
  local target_depth = crawl.random_range(2,3) -- TODO: This range can be higher if the callback has the coords, if we're near the edge then bailout

  -- Callback function
  local function callback(state)
    if state.usage.normal == nil then return false end
    if state.usage.depth == nil or state.usage.depth > target_depth then return false end
    -- TODO: Should also check if the coord is within a certain distance of map edge as an emergency bailout
    -- ... but we don't have the coord here right now
    if which then
      if state.usage.normal.y == 0 then
        return true
      end
      return false
    end
    if state.usage.normal.x == 0 then
      return true
    end
    return false

  end
  build_vaults_maze_layout(e,callback, "Bifur Maze")
end
-- TODO: Spirally maze

-- Builds the paint array for omnigrid
function layout_primitive_omnigrid()

  local gxm,gym = dgn.max_bounds()
  local options = {
    subdivide_initial_chance = 100, -- % chance of subdividing at first level, if < 100 then we might just get chaotic city
    subdivide_level_multiplier = 0.80,   -- Multiply subdivide chance by this amount with each level
    minimum_size = 15,  -- Don't ever create areas smaller than this
    fill_chance = 64, -- % chance of filling an area vs. leaving it open
    fill_padding = 2,  -- Padding around a fill area, effectively this is half the corridor width
    jitter_min = -1,
    jitter_max = 2
  }
  local results = omnigrid_subdivide_area(1,1,gxm-2,gym-2,options)
  local paint = {}
  local jitter = crawl.coinflip()
  for i, area in ipairs(results) do
    table.insert(paint,{ type = "floor", corner1 = {x = area.x1, y = area.y1}, corner2 = { x = area.x2, y = area.y2 }})
    -- Fill the area? -- TODO: We should do something to ensure at least one area is filled. Otherwise it's just another chaotic_city ...
    if crawl.random2(100) < options.fill_chance then
      local corner1 = { x = area.x1 + options.fill_padding, y = area.y1 + options.fill_padding }
      local corner2 = { x = area.x2 - options.fill_padding, y = area.y2 - options.fill_padding }
      -- Perform jitter
      if jitter then
        corner1.x = corner1.x + crawl.random_range(options.jitter_min,options.jitter_max)
        corner1.y = corner1.y + crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.x = corner2.x - crawl.random_range(options.jitter_min,options.jitter_max)
        corner2.y = corner2.y - crawl.random_range(options.jitter_min,options.jitter_max)
      end

      table.insert(paint,{ type = "wall", corner1 = corner1, corner2 = corner2 } )
    end
  end
  return paint
end

function omnigrid_subdivide_area(x1,y1,x2,y2,options,results,chance)
  -- Default parameters
  if results == nil then results = { } end
  if chance == nil then chance = options.subdivide_initial_chance end

  local subdiv_x, subdiv_y, subdivide = true,true,true
  local width,height = x2-x1+1,y2-y1+1

  -- Check which if any axes can be subdivided

  if width < 2 * options.minimum_size then
    subdiv_x = false
  end
  if height < 2 * options.minimum_size then
    subdiv_y = false
  end
  if not subdiv_x and not subdiv_y then
    subdivide = false
  end

  if crawl.random2(100) >= chance then
    subdivide = false
  end

  if not subdivide then
    -- End of subdivision; add an area
    table.insert(results, { x1=x1,y1=y1,x2=x2,y2=y2 })
  else
    -- Choose axis? (Remember some might already be too small)
    local which = "x"
    if not subdiv_x then which = "y"
    elseif subdiv_y then
      if crawl.coinflip() then which = "y" end
    end

    local new_chance = chance * options.subdivide_level_multiplier

    -- Could probably avoid this duplication but it's not that bad
    if which == "x" then
      local pos = crawl.random_range(options.minimum_size,width-options.minimum_size)
      -- Create the two new areas
      omnigrid_subdivide_area(x1,y1,x1 + pos - 1,y2,options,results,new_chance)
      omnigrid_subdivide_area(x1 + pos,y1,x2,y2,options,results,new_chance)
    else
      local pos = crawl.random_range(options.minimum_size,height-options.minimum_size)
      -- Create the two new areas
      omnigrid_subdivide_area(x1,y1,x2,y1 + pos - 1,options,results,new_chance)
      omnigrid_subdivide_area(x1,y1 + pos,x2,y2,options,results,new_chance)
    end
  end

  return results

end
