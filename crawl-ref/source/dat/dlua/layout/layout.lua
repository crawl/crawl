------------------------------------------------------------------------------
-- layout.lua: General layout functions and utilities
--
------------------------------------------------------------------------------

layout = {}

layout.area_in_bounds = function(bounds,opt)

  local minpadx,minpady,minsizex,minsizey = 0,0,1,1

  if opt.min_pad ~= nil then minpadx,minpady = opt.min_pad,opt.min_pad end
  if opt.min_size ~= nil then minsizex,minsizey = opt.min_size,opt.min_size end

  local width,height = bounds.x2-bounds.x1+1,bounds.y2-bounds.y1+1
  local maxx,maxy = width - 2*minpadx, height - 2*minpady

  if maxx<minsizex or maxy<minsizey then return nil end

  local sizex,sizey = crawl.random_range(minsizex,maxx),crawl.random_range(minsizey,maxy)
  local posx,posy = crawl.random_range(0,maxx-sizex),crawl.random_range(0,maxy-sizey)
  return { x1 = bounds.x1 + minpadx + posx, y1 = bounds.y1 + minpady + posy,
           x2 = bounds.x1 + minpadx + posx + sizex - 1, y2 = bounds.y1 + minpady + posy + sizey - 1 }

end


--
--  get_areas_around_primary_vault
--
--  This function divides the map into up to 4 rectangular areas
--    around the primary vault.
--
-- Parameter(s):
--  -> e: A reference to the global environment. Pass in _G.
--  -> x1
--  -> y1
--  -> x2
--  -> y2: The maximum dimensions of the area to consider.
--
-- Returns: An array containing the areas. If there is no
--          primary vault, the array will contain one area
--          bounded by x1, y1, x2, and y2. If the primary
--          vault covers evrything (e.g. an encompass vault) the
--          array will contain zero areas. Otherwise, the array
--          will contain 1 or more areas composing a
--          non-overlapping subset of the area bounded by x1,
--          y1, x2, and y2.
--
-- The area data structure has
--  -> x_min,
--     x_max,
--     y_min,
--     y_max: The inclusive bounds of the area. The area
--            includes position (x_max, y_max).
--  -> x_min_is_soft,
--     x_max_is_soft,
--     y_min_is_soft,
--     y_max_is_soft: Whether each of the walls is "soft"
--
--  The 4 edges of each area will each be designated "hard" or
--    "soft". An edge that only borders another open area is
--    soft, while all other edges are hard. The layout may
--    want to treat soft edges differently.
--
--  Example of area division:
--    /////////////////////////////////
--    //+---------------------------+//
--    //|                           |//
--    //|           AREA 1          |//
--    //|                           |//
--    //+--------+---------+--------+//
--    //|        |/////////|        |//
--    //|        |/PRIMARY/|        |//
--    //|        |/////////| AREA 4 |//
--    //|        |//VAULT//|        |//
--    //| AREA 2 |/////////|        |//
--    //|        +---------+--------+//
--    //|        |                  |//
--    //|        |      AREA 3      |//
--    //|        |                  |//
--    //+--------+------------------+//
--    /////////////////////////////////
--
--  In this example, the following edges are soft:
--    -> Area 1: None
--    -> Area 2: y_min (top)
--    -> Area 3: x_min (left)
--    -> Area 4: y_min (top) and y_max (bottom)
--

layout.get_areas_around_primary_vault = function(e, x1, y1, x2, y2)

  function set_all_edges_soft (area)
    area.x_min_is_soft = false
    area.x_max_is_soft = false
    area.y_min_is_soft = false
    area.y_max_is_soft = false
  end

  local areas = {}

  -- get dimensions of primary vault
  local p_x_min, p_x_max, p_y_min, p_y_max = e.primary_vault_dimensions()

  if (p_x_min == nil) then
    -- if there is no primary vault, just use the total size

    areas[1] = {}
    areas[1].x_min = x1
    areas[1].x_max = x2
    areas[1].y_min = y1
    areas[1].y_max = y2
    set_all_edges_soft(areas[1])

  else
    -- if there is a primary vault, divide up the area

    -- 1. Decide which areas we need

    local area_x_min = nil
    if (p_x_min > x1) then
      area_x_min = {}
      area_x_min.x_min = x1
      area_x_min.x_max = p_x_min - 1
      area_x_min.y_min = math.max(y1, p_y_min)
      area_x_min.y_max = math.min(y2, p_y_max)
      set_all_edges_soft(area_x_min)
      areas[#areas + 1] = area_x_min
    end

    local area_x_max = nil
    if (p_x_max < x2) then
      area_x_max = {}
      area_x_max.x_min = p_x_max + 1
      area_x_max.x_max = x2
      area_x_max.y_min = math.max(y1, p_y_min)
      area_x_max.y_max = math.min(y2, p_y_max)
      set_all_edges_soft(area_x_max)
      areas[#areas + 1] = area_x_max
    end

    local area_y_min = nil
    if (p_y_min > y1) then
      area_y_min = {}
      area_y_min.x_min = math.max(x1, p_x_min)
      area_y_min.x_max = math.min(x2, p_x_max)
      area_y_min.y_min = y1
      area_y_min.y_max = p_y_min - 1
      set_all_edges_soft(area_y_min)
      areas[#areas + 1] = area_y_min
    end

    local area_y_max = nil
    if (p_y_max < y2) then
      area_y_max = {}
      area_y_max.x_min = math.max(x1, p_x_min)
      area_y_max.x_max = math.min(x2, p_x_max)
      area_y_max.y_min = p_y_max + 1
      area_y_max.y_max = y2
      set_all_edges_soft(area_y_max)
      areas[#areas + 1] = area_y_max
    end

    -- 2. Expand an area to cover each needed corner
    --    -> we choose which area to expand at random
    --    -> the unexpanded area gets a soft wall

    if (area_x_min ~= nil and area_y_min ~= nil) then
      if (crawl.coinflip()) then
        area_x_min.y_min = area_y_min.y_min
        area_y_min.x_min_is_soft = true
      else
        area_y_min.x_min = area_x_min.x_min
        area_x_min.y_min_is_soft = true
      end
    end

    if (area_x_min ~= nil and area_y_max ~= nil) then
      if (crawl.coinflip()) then
        area_x_min.y_max = area_y_max.y_max
        area_y_max.x_min_is_soft = true
      else
        area_y_max.x_min = area_x_min.x_min
        area_x_min.y_max_is_soft = true
      end
    end

    if (area_x_max ~= nil and area_y_min ~= nil) then
      if (crawl.coinflip()) then
        area_x_max.y_min = area_y_min.y_min
        area_y_min.x_max_is_soft = true
      else
        area_y_min.x_max = area_x_max.x_max
        area_x_max.y_min_is_soft = true
      end
    end

    if (area_x_max ~= nil and area_y_max ~= nil) then
      if (crawl.coinflip()) then
        area_x_max.y_max = area_y_max.y_max
        area_y_max.x_max_is_soft = true
      else
        area_y_max.x_max = area_x_max.x_max
        area_x_max.y_max_is_soft = true
      end
    end

  end

  return areas
end
