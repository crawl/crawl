------------------------------------------------------------------------------
-- v_paint.lua:
--
-- Functions for painting the Vaults layouts and managing
-- walls and areas that rooms can be applied to.
--
-- We will (in the simplest case) paint rectangles of floor onto the dungeon
-- and
------------------------------------------------------------------------------

require("dlua/v_paint.lua")
require("dlua/v_rooms.lua")
require("dlua/v_layouts.lua")

-- Old code for reference featuring randomisation
function build_vaults_ring_level_dud(e)
    if not crawl.game_started() then return end

    local gxm, gym = dgn.max_bounds()
--    e.extend_map{width = gxm, height = gym}
    e.layout_type "vaults_ring"

    -- Fill whole dungeon with stone
    dgn.fill_grd_area(0, 0, gxm - 1, gym - 1, "stone_wall")

    -- Initialise glyphs
    local floor = '.'
    local walk_wall = crawl.random_element { ['v'] = 6, ['c'] = 4, ['b'] = 1 }
    local vault_wall = crawl.random_element { ['v'] = 6, ['c'] = 4, ['b'] = 1 }

    -- Create the walkway
    local padMin,padMax,walkMin,walkMax,roomsOuter,roomsInner = 15,20,3,8,true,true

    -- Outer padding
    local paddingX = crawl.random_range(padMin,padMax) --crawl.random_range(12,16)
    local paddingY = crawl.random_range(padMin,padMax) --crawl.random_range(12,16)

    -- Widths of walkways
    local walkWidth1 = crawl.random_range(walkMin,walkMax)
    local walkWidth2 = crawl.random_range(walkMin,walkMax)
    local walkHeight1 = crawl.random_range(walkMin,walkMax)
    local walkHeight2 = crawl.random_range(walkMin,walkMax)

    local walkX11 = paddingX
    local walkX21 = gxm - paddingX
    local walkX12 = paddingX + walkWidth1
    local walkX22 = gxm - paddingX - walkWidth2
    local walkY11 = paddingY
    local walkY21 = gym - paddingY
    local walkY12 = paddingY + walkHeight1
    local walkY22 = gym - paddingY - walkHeight2

  --  dgn.fill_grd_area(1, 1, gxm - 2, gym - 2, "stone_wall")

--    e.fill_area{x1=walkX11, y1=walkY11, x2=walkX21, y2=walkY21, fill=floor }
  --  e.fill_area{x1=walkX12, y1=walkY12, x2=walkX22, y2=walkY22, fill='x'}

    dgn.fill_grd_area(walkX11, walkY11, walkX21, walkY21, "floor")
    dgn.fill_grd_area(walkX12, walkY12, walkX22, walkY22, "stone_wall")

    if crawl.one_chance_in(4) then
      -- TODO: Flood a corridor patch with water
    end

    -- Populate walls with vaults
    local eligible_walls = {
      { walkX11,walkY11,walkX21-walkX11+1,{x=0,y=-1},1 }, -- North outer
      { walkX21,walkY11,walkY21-walkY11+1,{x=1,y=0},1  }, -- East outer
      { walkX21,walkY21,walkX21-walkX11+1,{x=0,y=1},1  }, -- South outer
      { walkX11,walkY21,walkY21-walkY11+1,{x=-1,y=0},1  }, -- West outer

      { walkX22,walkY12-1, walkX22-walkX12+1,{x=0,y=1},1 }, -- North inner
      { walkX22+1,walkY22, walkY22-walkY12+1,{x=-1,y=0},1 }, -- East inner
      { walkX12,walkY22+1, walkX22-walkX12+1,{x=0,y=-1},1 }, -- South inner
      { walkX12-1,walkY12, walkY22-walkY12+1,{x=1,y=0},1 } -- West inner
    }
    return eligible_walls

end

