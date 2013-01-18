------------------------------------------------------------------------------
-- v_layouts.lua:
--
-- Contains the functions that actually build the layouts.
-- See v_main.lua for further information.
--
-- by mumra
------------------------------------------------------------------------------

require("dlua/v_paint.lua")
require("dlua/v_rooms.lua")

function build_vaults_ring_layout(e, corridorWidth, outerPadding)

  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

  print("Ring Layout")

  local c1 = { x = outerPadding, y = outerPadding }
  local c2 = { x = outerPadding + corridorWidth - 1, y = outerPadding + corridorWidth - 1 }
  local c3 = { x = gxm - outerPadding - corridorWidth, y = gym - outerPadding - corridorWidth }
  local c4 = { x = gxm - outerPadding - 1, y = gym - outerPadding - 1 }

  -- Paint four floors
  local paint = {
    { type = "floor", corner1 = c1, corner2 = { x = c4.x, y = c2.y } },
    { type = "floor", corner1 = c1, corner2 = { x = c2.x, y = c4.y } },
    { type = "floor", corner1 = { x = c1.x, y = c3.y } , corner2 = c4 },
    { type = "floor", corner1 = { x = c3.x, y = c1.y } , corner2 = c4 }
  }
  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, 3)

end

function build_vaults_cross_layout(e, corridorWidth, intersect)

  -- Ignoring intersect for now
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

    print("Cross Layout")

  local xc = math.floor((gxm-corridorWidth)/2)
  local yc = math.floor((gym-corridorWidth)/2)
  local paint = {
    { type = "floor", corner1 = { x = xc, y = 0 }, corner2 = { x = xc + corridorWidth - 1, y = gym - 1 } },
    { type = "floor", corner1 = { x = 0, y = yc }, corner2 = { x = gxm - 1, y = yc + corridorWidth - 1 } }
  }

  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, 4)

end

function build_vaults_big_room_layout(e, outerPadding)
  -- The Big Room
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()


    print("Big Room Layout")

  -- Will have a ring of outer rooms but the central area will be chaotic city
  local paint = {
    { type = "floor", corner1 = { x = outerPadding, y = outerPadding }, corner2 = { x = gxm - outerPadding - 1, y = gym - outerPadding - 1 } }
  }
  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, 4)

end

function build_vaults_chaotic_city_layout(e)
  if not crawl.game_started() then return end
  local gxm, gym = dgn.max_bounds()

      print("Chaotic City Layout")


  -- Paint everything with floor
  local paint = {
    { type = "floor", corner1 = { x = 0, y = 0 }, corner2 = { x = gxm - 1, y = gym - 1 } }
  }

  local data = paint_vaults_layout(e, paint)
  local rooms = place_vaults_rooms(e, data, 20, 5)

end