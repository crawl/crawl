------------------------------------------------------------------------------
-- minimum_map_area.lua: Constants and a function to enforce minimum map size.
--
------------------------------------------------------------------------------

minimum_map_area = {}

--
--  The minimum area for each map type
--

minimum_map_area.BASE_AREA    = 10

minimum_map_area.CORRIDORS    = minimum_map_area.BASE_AREA * 0.75
minimum_map_area.ROOMS        = minimum_map_area.BASE_AREA
minimum_map_area.PASSAGES     = minimum_map_area.BASE_AREA
minimum_map_area.NARROW_CAVES = minimum_map_area.BASE_AREA
minimum_map_area.OPEN_CAVES   = minimum_map_area.BASE_AREA * 1.5
minimum_map_area.CITY         = minimum_map_area.BASE_AREA * 2
minimum_map_area.OPEN         = minimum_map_area.BASE_AREA * 2
minimum_map_area.DIVISIONS    = minimum_map_area.BASE_AREA * 1.5
minimum_map_area.SWAMP        = minimum_map_area.BASE_AREA



--
-- is_map_big_enough
--
-- This function returns whether the map is big enough to be a
--  map of the specified type.
--
-- Parameter(s):
--  -> e: A reference to the global environment. Pass in _G
--  -> min_pct: The minimum area as a percentage of grid area for this map type
--               Defaults to BASE_AREA
--  -> floor: The glyphs to treat as floor glyphs
--            Defaults to ".{([})]<>"
--
-- Returns: true if the map is large enough, false otherwise.
--

minimum_map_area.is_map_big_enough = function (e, min_pct, floor)
    if min_pct == nil then
        min_pct = BASE_AREA
    end
    if floor == nil then
        floor = ".{([})]<>"
    end

    local min_area = (min_pct * e.width() * e.height()) / 100

    return e.count_feature_in_box { feat = floor } > min_area
end
