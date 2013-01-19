------------------------------------------------------------------------------
-- v_layouts.lua:
--
-- Main layouts include for Vaults.
--
-- Code by mumra
-- Based on work by infiniplex, based on original work and designs by mumra :)
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

require("dlua/v_paint.lua")
require("dlua/v_rooms.lua")

function build_vaults_ring_layout(e, corridorWidth, outerPadding)

  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

  print("Ring Layout")

  local c1 = { x = outerPadding, y = outerPadding }
  local c2 = { x = outerPadding + corridorWidth, y = outerPadding + corridorWidth }
  local c3 = { x = gxm - outerPadding - corridorWidth - 1, y = gym - outerPadding - corridorWidth - 1 }
  local c4 = { x = gxm - outerPadding - 1, y = gym - outerPadding - 1 }

  -- Paint four floors
  local paint = {
    { type = "floor", corner1 = c1, corner2 = c4},
    { type = "wall", corner1 = c2, corner2 = c3 }
  }
  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, { max_room_depth = 3 })

end

function build_vaults_cross_layout(e, corridorWidth, intersect)

  -- Ignoring intersect for now
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

    print("Cross Layout")

  local xc = math.floor((gxm-corridorWidth)/2)
  local yc = math.floor((gym-corridorWidth)/2)
  local paint = {
    { type = "floor", corner1 = { x = xc, y = 1 }, corner2 = { x = xc + corridorWidth - 1, y = gym - 2 } },
    { type = "floor", corner1 = { x = 1, y = yc }, corner2 = { x = gxm - 2, y = yc + corridorWidth - 1 } }
  }

  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, { max_room_depth = 4 })

end

function build_vaults_big_room_layout(e, outerPadding)
  -- The Big Room
  -- Sorry, no imps
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()


    print("Big Room Layout")

  -- Will have a ring of outer rooms but the central area will be chaotic city
  local paint = {
    { type = "floor", corner1 = { x = outerPadding, y = outerPadding }, corner2 = { x = gxm - outerPadding - 1, y = gym - outerPadding - 1 } }
  }
  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, { max_room_depth = 4 })

end

function build_vaults_chaotic_city_layout(e)
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

      print("Chaotic City Layout")

  -- Paint everything with floor
  local paint = {
    { type = "floor", corner1 = { x = 1, y = 1 }, corner2 = { x = gxm - 2, y = gym - 2 } }
  }

  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, { max_room_depth = 5 })

end