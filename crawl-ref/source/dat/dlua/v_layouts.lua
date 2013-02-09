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

hypervaults = {}  -- Main namespace

require("dlua/v_debug.lua")
require("dlua/v_paint.lua")
require("dlua/v_rooms.lua")
require("dlua/v_shapes.lua")

_VAULTS_DEBUG = true

-- The four directions and their associated vector normal and name.
-- This helps us manage orientation of rooms.
hypervaults.normals = {
  { x = 0, y = -1, dir = 0, name="n" },
  { x = -1, y = 0, dir = 1, name="w" },
  { x = 0, y = 1, dir = 2, name="s" },
  { x = 1, y = 0, dir = 3, name="e" }
}

-- Default parameters for all Vaults layouts. Some individual layouts might
-- tweak these parameters to create specific effects.
function vaults_default_options()

  local options = {
    min_distance_from_wall = 2, -- Room must be at least this far from outer walls (in open areas). Reduces chokepoints.
    max_rooms = 27, -- Maximum number of rooms to attempt to place
    max_room_depth = 0, -- No max depth (not implemented yet anyway)
    -- The following settings (in addition to max_rooms) can adjust how long it takes to build a level, at the expense of potentially less intereting layouts
    max_room_tries = 27, -- How many *consecutive* rooms can fail to place before we exit the entire routine
    max_place_tries = 50, -- How many times we will attempt to place *each room* before picking another

    -- Weightings of various types of room generators.
    room_type_weights = {
      { generator = "code", paint_callback = floor_vault_paint_callback, weight = 30, min_size = 6, max_size = 12 }, -- Floor vault
      { generator = "code", paint_callback = junction_vault_paint_callback, weight = 80, min_size = 6, max_size = 30, min_corridor = 2, max_corridor = 8 }, -- Floor vault ++
      { generator = "tagged", tag = "vaults_room", weight = 50, max_rooms = 6 },
      { generator = "tagged", tag = "vaults_empty", weight = 40 },
      { generator = "tagged", tag = "vaults_hard", weight = 10, max_rooms = 1 },
      { generator = "tagged", tag = "vaults_entry_crypt", weight = (you.where() == dgn.level_name(dgn.br_entrance("Crypt"))) and 25 or 0, max_rooms = 1 },
      { generator = "tagged", tag = "vaults_entry_blade", weight = (you.where() == dgn.level_name(dgn.br_entrance("Blade"))) and 25 or 0, max_rooms = 1 },
    },

    -- Weightings for types of wall to use across the whole layout
    -- TODO: These weights should vary by depth; metal/green crystal become much more likely at V:3-4
    layout_wall_weights = {
      { feature = "rock_wall", weight = 2 }, -- Possibly shouldn't be here at all, someone please advise
      { feature = "stone_wall", weight = 30 },
      { feature = "metal_wall", weight = 20 },
      { feature = "green_crystal_wall", weight = 5 },
    },
    layout_floor_type = "floor"

  }

  return options
end

function dis_default_options()

  local dis_options = {
    min_distance_from_wall = 1, -- Room must be at least this far from outer walls (in open areas). Reduces chokepoints.
    max_rooms = 50, -- Maximum number of rooms to attempt to place
    max_room_tries = 30, -- How many *consecutive* rooms can fail to place before we exit the entire routine
    max_place_tries = 100, -- How many times we will attempt to place *each room* before picking another

    -- Squares and junctions
    room_type_weights = {
      { generator = "code", paint_callback = floor_vault_paint_callback, weight = 10, min_size = 10, max_size = 20 }, -- Floor vault
      { generator = "code", paint_callback = junction_vault_paint_callback, weight = 80, min_size = 4, max_size = 40, min_corridor = 1, max_corridor = 10 }, -- Floor vault ++
    },

    -- We only have metal in Dis
    layout_wall_weights = {
      { feature = "metal_wall", weight = 20 },
    },

    decorate_walls_callback = function(room, connections, door_required, has_windows)
                          -- 1/4 of walls have doors, the rest become floor
                          if crawl.one_chance_in(4) then
                            hypervaults.rooms.decorate_walls(room, connections, door_required, has_windows)
                          else
                            hypervaults.rooms.decorate_walls_open(room, connections, door_required, has_windows)
                          end
                        end

  }

  return merge_options(hypervaults.default_options(),dis_options)

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
-- but alternately should be able to wire room generators to create initial terrain. Since rooms can now be creaated from paint arrays _anyway_,
-- it seems that pre-painting the layout is completely unneccesary and room generation can do anything ...
function hypervaults.build_layout(e, name, paint, options)
  if not crawl.game_started() then return end

  if _VAULTS_DEBUG then print("Hypervaults Layout: " .. name) end

  e.layout_type "hypervaults" -- TODO: Lowercase and underscorise the name?

  local data = paint_vaults_layout(paint, options)
  local rooms = place_vaults_rooms(e, data, options.max_rooms, options)
end

-- Merges together two options tables (usually the default options getting
-- overwritten by a minimal set provided by an individual layout)
function merge_options(base,to_merge)
  -- Quite simplistic, right now this won't recurse into sub tables (it might need to)
  for key, val in ipairs(to_merge) do
    base[key] = val
  end
  return base
end

-- Build any vaults layout from a paint array, useful to quickly prototype
-- layouts in layout_vaults.des
function build_vaults_layout(e, name, paint, options)

  if not crawl.game_started() then return end

  if _VAULTS_DEBUG then print("Vaults Layout: " .. name) end

  e.layout_type "vaults" -- TODO: Lowercase and underscorise the name parameter?

  local defaults = vaults_default_options()
  if options ~= nil then merge_options(defaults,options) end

  local data = paint_vaults_layout(paint, defaults)
  local rooms = place_vaults_rooms(e, data, defaults.max_rooms, defaults)

end

function build_vaults_ring_layout(e, corridorWidth, outerPadding)

  if not crawl.game_started() then return end
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
  if not crawl.game_started() then return end
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
  -- Sorry, no imps
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()
  local padx,pady = crawl.random_range(minPadding,maxPadding),crawl.random_range(minPadding,maxPadding)

  -- Will have a ring of outer rooms but the central area will be chaotic city
  local paint = {
    { type = "floor", corner1 = { x = padx, y = pady }, corner2 = { x = gxm - padx - 1, y = gym - pady - 1 } }
  }
  build_vaults_layout(e, "Big Room", paint, { max_room_depth = 4 })

end

function build_vaults_chaotic_city_layout(e)
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

  -- Paint entire level with floor
  local paint = {
    { type = "floor", corner1 = { x = 1, y = 1 }, corner2 = { x = gxm - 2, y = gym - 2 } }
  }

  build_vaults_layout(e, "Choatic City", paint, { max_room_depth = 5, max_rooms = 30 })

end

function build_vaults_maze_layout(e,veto_callback, name)
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()
  if name == nil then name = "Maze" end

  -- Put a single empty room somewhere roughly central. All rooms will be built off from each other following this
  local x1 = crawl.random_range(30, gxm-70)
  local y1 = crawl.random_range(30, gym-70)

  local paint = {
    { type = "floor", corner1 = { x = x1, y = y1 }, corner2 = { x = x1 + crawl.random_range(4,10), y = y1 + crawl.random_range(4,10) } }
  }

  local function room_veto(room)
    -- Avoid *very* large empty rooms, but they can be long and thin
    if room.type == "empty" then
      if room.size.x + room.size.y > 23 then return true end
    end
    return false
  end

  build_vaults_layout(e, name, paint, { max_room_depth = 0, veto_place_callback = veto_callback })

end
-- Uses a custom veto function to alternate between up or down connections every {dist} rooms
function build_vaults_maze_snakey_layout(e)

  local which = 0
  if crawl.coinflip() then which = 1 end

  -- How many rooms to draw before alternating
  -- When it's 1 it doesn't create an obvious visual effect but hopefully in gameplay
  -- it will be more obvious.
  local dist = crawl.random_range(1,2) -- TODO: This range can be higher if the callback has the coords, if we're near the edge then bailout
  -- Just to be fair we'll offset by a number, this will determine how far from the
  -- first room it goes before the first switch
  local offset = crawl.random_range(0,dist-1)
  -- Callback function
  local function callback(usage,room)
    if usage.normal == nil or usage.depth == nil then return false end
    local odd = (math.floor(usage.depth/dist)+offset) % 2
    if odd == which then
      if usage.normal.y == 0 then return true end
      return false
    end
    if usage.normal.x == 0 then return true end
    return false
  end
  build_vaults_maze_layout(e,callback, "Snakey Maze")
end

-- Goes just either horizontally or vertically for a few rooms then branches out, makes
-- an interesting sprawly maze with two bulby areas
function build_vaults_maze_bifur_layout(e)

  local which = crawl.coinflip()
  local target_depth = crawl.random_range(2,3) -- TODO: This range can be higher if the callback has the coords, if we're near the edge then bailout

  -- Callback function
  local function callback(usage,room)
    if usage.normal == nil then return false end
    if usage.depth == nil or usage.depth > target_depth then return false end
    -- TODO: Should also check if the coord is within a certain distance of map edge as an emergency bailout
    -- ... but we don't have the coord here right now
    if which then
      if usage.normal.y == 0 then
        return true
      end
      return false
    end
    if usage.normal.x == 0 then
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
    fill_chance = 75, -- % chance of filling an area vs. leaving it open
    fill_padding = 2,  -- Padding around a fill area, effectively this is half the corridor width
  }
  local results = omnigrid_subdivide_area(1,1,gxm-2,gym-2,options)
  local paint = {}
  for i, area in ipairs(results) do
    -- TODO: Jitter / vary corridor widths (depending on size of area)
    table.insert(paint,{ type = "floor", corner1 = {x = area.x1, y = area.y1}, corner2 = { x = area.x2, y = area.y2 }})
    -- Fill the area?
    if crawl.random2(100) < options.fill_chance then
      table.insert(paint,{ type = "wall", corner1 = { x = area.x1 + options.fill_padding, y = area.y1 + options.fill_padding }, corner2 = { x = area.x2 - options.fill_padding, y = area.y2 - options.fill_padding }} )
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

-- TODO: Some redundant code here, combine
function build_dis_layout(e, name, paint, options)

  if not crawl.game_started() then return end

  if _VAULTS_DEBUG then print("Dis Layout: " .. name) end

  e.layout_type "dis" -- TODO: Lowercase and underscorise the name parameter?

  local defaults = dis_default_options()
  if options ~= nil then merge_options(defaults,options) end

  local data = paint_vaults_layout(paint, defaults)
  local rooms = place_vaults_rooms(e, data, defaults.max_rooms, defaults)

end

function build_dis_maze_layout(e)
  if not crawl.game_started() then return end
  local name = "Maze"
  local paint = n3v_paint_small_central_room()

  build_dis_layout(e, name, paint, { max_room_depth = 0 })
end
