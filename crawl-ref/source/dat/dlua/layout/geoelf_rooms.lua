------------------------------------------------------------------------------
-- geoelf_rooms.lua:
--
-- Constants and functions for drawing rooms.
------------------------------------------------------------------------------

geoelf.rooms        = {}    -- Namespace for room drawing functions
geoelf.rooms.mirror = {}    -- Namespace for room mirroring constants
geoelf.rooms.shape  = {}    -- Namespace for room shape constants

crawl_require("dlua/layout/geoelf_directions.lua")
crawl_require("dlua/layout/geoelf_glyphs.lua")

----------------
--
-- The room mirror states
--
-- Rooms can be mirrored in three directions, for a total of 8
--  possible combinations. The D indicates mirroring diagonally
--  (i,e, swapping x and y).
--

geoelf.rooms.mirror.NONE = 0
geoelf.rooms.mirror.X    = 1
geoelf.rooms.mirror.Y    = 2
geoelf.rooms.mirror.XY   = 3
geoelf.rooms.mirror.D    = 4
geoelf.rooms.mirror.DX   = 5
geoelf.rooms.mirror.DY   = 6
geoelf.rooms.mirror.DXY  = 7

----------------
--
-- The room shapes
--
-- The passage requirements mean that square, diamond, and
--  cross rooms can only rarely be placed. They get a high
--  weight so they still appear on the map. This is especially
--  important for diamond, because it has to compete with the
--  hexagon room shapes.
--
-- The direction for the triangle indicates which way the apex
--  points. The base is on the opposite side
--
-- The directions for the hexagons indicate which direction the
--  flat sides face.
--
-- Octagons always fit, so they get a low weight. However,
--  they still appear quite frequently because they serve as a
--  de facto default room shape.
--

geoelf.rooms.shape.COUNT      = 10
geoelf.rooms.shape.SQUARE     = 0
geoelf.rooms.shape.DIAMOND    = 1
geoelf.rooms.shape.CROSS      = 2
geoelf.rooms.shape.TRIANGLE_S = 3
geoelf.rooms.shape.TRIANGLE_N = 4
geoelf.rooms.shape.TRIANGLE_E = 5
geoelf.rooms.shape.TRIANGLE_W = 6
geoelf.rooms.shape.HEXAGON_NS = 7
geoelf.rooms.shape.HEXAGON_EW = 8
geoelf.rooms.shape.OCTAGON    = 9

geoelf.rooms.shape.BASE_FREQUENCY =
  { [geoelf.rooms.shape.SQUARE]     = 8,
    [geoelf.rooms.shape.DIAMOND]    = 25,
    [geoelf.rooms.shape.CROSS]      = 4,
    [geoelf.rooms.shape.TRIANGLE_S] = 2,
    [geoelf.rooms.shape.TRIANGLE_N] = 2,
    [geoelf.rooms.shape.TRIANGLE_E] = 2,
    [geoelf.rooms.shape.TRIANGLE_W] = 2,
    [geoelf.rooms.shape.HEXAGON_NS] = 5,
    [geoelf.rooms.shape.HEXAGON_EW] = 5,
    [geoelf.rooms.shape.OCTAGON]    = 1 }

geoelf.rooms.shape.CONNECTIONS =
  { [geoelf.rooms.shape.SQUARE] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = false,
        [geoelf.directions.NW] = false,
        [geoelf.directions.SW] = false,
        [geoelf.directions.NE] = false },
    [geoelf.rooms.shape.DIAMOND] =
      { [geoelf.directions.S]  = false,
        [geoelf.directions.N]  = false,
        [geoelf.directions.E]  = false,
        [geoelf.directions.W]  = false,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = true },
    [geoelf.rooms.shape.CROSS] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = false,
        [geoelf.directions.NW] = false,
        [geoelf.directions.SW] = false,
        [geoelf.directions.NE] = false },
    [geoelf.rooms.shape.TRIANGLE_S] =
      { [geoelf.directions.S]  = false,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = false,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = false },
    [geoelf.rooms.shape.TRIANGLE_N] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = false,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = false,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = false,
        [geoelf.directions.NE] = true },
    [geoelf.rooms.shape.TRIANGLE_E] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = false,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = false,
        [geoelf.directions.SW] = false,
        [geoelf.directions.NE] = true },
    [geoelf.rooms.shape.TRIANGLE_W] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = false,
        [geoelf.directions.SE] = false,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = false },
    [geoelf.rooms.shape.HEXAGON_NS] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = false,
        [geoelf.directions.W]  = false,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = true },
    [geoelf.rooms.shape.HEXAGON_EW] =
      { [geoelf.directions.S]  = false,
        [geoelf.directions.N]  = false,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = true },
    [geoelf.rooms.shape.OCTAGON] =
      { [geoelf.directions.S]  = true,
        [geoelf.directions.N]  = true,
        [geoelf.directions.E]  = true,
        [geoelf.directions.W]  = true,
        [geoelf.directions.SE] = true,
        [geoelf.directions.NW] = true,
        [geoelf.directions.SW] = true,
        [geoelf.directions.NE] = true } }

geoelf.rooms.shape.MIRRORINGS =
  { [geoelf.rooms.shape.SQUARE] =
      { {geoelf.rooms.mirror.NONE, 1}, -- why are these set up as weighted?
        {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.Y,    1},
        {geoelf.rooms.mirror.XY,   1},
        {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1},
        {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} },
    [geoelf.rooms.shape.DIAMOND] =
      { {geoelf.rooms.mirror.NONE, 1},
        {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.Y,    1},
        {geoelf.rooms.mirror.XY,   1},
        {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1},
        {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} },
    [geoelf.rooms.shape.CROSS] =
      { {geoelf.rooms.mirror.NONE, 1},
        {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.Y,    1},
        {geoelf.rooms.mirror.XY,   1},
        {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1},
        {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} },
    [geoelf.rooms.shape.TRIANGLE_S] =
      { {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1} },
    [geoelf.rooms.shape.TRIANGLE_N] =
      { {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} },
    [geoelf.rooms.shape.TRIANGLE_E] =
      { {geoelf.rooms.mirror.NONE, 1},
        {geoelf.rooms.mirror.Y,    1} },
    [geoelf.rooms.shape.TRIANGLE_W] =
      { {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.XY,   1} },
    [geoelf.rooms.shape.HEXAGON_NS] =
      { {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1},
        {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} },
    [geoelf.rooms.shape.HEXAGON_EW] =
      { {geoelf.rooms.mirror.NONE, 1},
        {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.Y,    1},
        {geoelf.rooms.mirror.XY,   1} },
    [geoelf.rooms.shape.OCTAGON] =
      { {geoelf.rooms.mirror.NONE, 1},
        {geoelf.rooms.mirror.X,    1},
        {geoelf.rooms.mirror.Y,    1},
        {geoelf.rooms.mirror.XY,   1},
        {geoelf.rooms.mirror.D,    1},
        {geoelf.rooms.mirror.DX,   1},
        {geoelf.rooms.mirror.DY,   1},
        {geoelf.rooms.mirror.DXY,  1} } }



----------------------------------------------------------------
--
-- Choosing and drawing geometric rooms
--
--
-- The shape function produces a 2D array for the room indexed
--  with the room center at [0][0] (you can do that in LUA).
--  The meaning of the values in the array are as follows:
--    -> UNCHANGED: leave the existing glyhps alone
--    -> ROOM_OPEN: leave the corridor type on the floor if any,
--                  otherwise use geoelf.glyphs.FLOOR
--    -> ROOM_BORDER: perform border substituion
--    -> ROOM_OUTLINE: turns plantlike things and features into
--                     floor, everything else stays the same
--      -> We need this to avoid having features against the
--         wall when we cut into existing rooms
--    -> PLANTLIKE: becomes all trees, all bushes, all plants,
--                  or all fungus
--    -> FEATURE: becomes all statues or all fountains
--    -> GLASS_PLANTLIKE: as PLANTLIKE, but an image on a glass
--                        wall
--    -> GLASS_FEATURE: as FEATURE, but an image on a glass wall
--    -> Anything else is a glyph for the map
--      -> Using glyphs that are not in the lookup table will
--         cause nil values next time the cell is analyzed
--
-- The array also has a size field, which is the maximum index
--  (positive or negative). All elements in the square defined
--  by this value are set.
--

geoelf.rooms.UNCHANGED       = 0
geoelf.rooms.ROOM_OUTLINE    = 1
geoelf.rooms.ROOM_BORDER     = 2
geoelf.rooms.ROOM_OPEN       = 3
geoelf.rooms.PLANTLIKE       = 4
geoelf.rooms.FEATURE         = 5
geoelf.rooms.GLASS_PLANTLIKE = 6
geoelf.rooms.GLASS_FEATURE   = 7

function geoelf.rooms.draw (e, room_data, corridor_data, index,
                            fancy, force_open_room_edge)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (index >= room_data.count) then
    crawl.mpr("Error: index is past the end of room_data")
  end
  if (room_data[index] == nil) then
    crawl.mpr("Error: Room index does not exist")
  end

  local gxm, gym = dgn.max_bounds()

  local center_x = room_data[index].center_x
  local center_y = room_data[index].center_y
  local radius = room_data[index].radius
  if (geoelf.debug) then
    if (fancy) then
      print("  Drawing room " .. index .. " at " .. center_x .. ", " ..
            center_y .. " radius = " .. " fancy = true")
    else
      print("  Drawing room " .. index .. " at " .. center_x .. ", " ..
            center_y .. " radius = " .. " fancy = false")
    end
  end
  local shape = geoelf.rooms.choose_shape(room_data, corridor_data, index)

  -- calculate the room contents for each cell
  local room_glyphs
  if (shape == geoelf.rooms.shape.SQUARE) then
    room_glyphs = geoelf.rooms.square(radius, fancy)
  elseif (shape == geoelf.rooms.shape.DIAMOND) then
    room_glyphs = geoelf.rooms.diamond(radius, radius)
  elseif (shape == geoelf.rooms.shape.CROSS) then
    local indent = math.min(crawl.random2(radius - 2) + 2, radius - 1)
    room_glyphs = geoelf.rooms.cross(radius, indent, fancy)
  elseif (shape == geoelf.rooms.shape.TRIANGLE_S or
          shape == geoelf.rooms.shape.TRIANGLE_N or
          shape == geoelf.rooms.shape.TRIANGLE_E or
          shape == geoelf.rooms.shape.TRIANGLE_W) then
    room_glyphs = geoelf.rooms.triangle(radius, fancy)
  elseif (shape == geoelf.rooms.shape.HEXAGON_NS or
          shape == geoelf.rooms.shape.HEXAGON_EW) then
    room_glyphs = geoelf.rooms.hexagon(radius, fancy)
  elseif (shape == geoelf.rooms.shape.OCTAGON) then
    local diagonal_cutoff = math.floor(radius * 1.4 + crawl.random_real())
    room_glyphs = geoelf.rooms.octagon(radius, diagonal_cutoff, fancy)
  else
    crawl.mpr("Error: Trying to draw room of invalid shape " .. shape)
  end

  -- draw the room
  local mirror    = util.random_choose_weighted(
                              geoelf.rooms.shape.MIRRORINGS[shape])
  local feature   = util.random_choose_weighted(geoelf.glyphs.FEATURE_OPTIONS)
  local plantlike = util.random_choose_weighted(geoelf.glyphs.PLANTLIKE_OPTIONS)
  if (geoelf.debug and fancy) then
    print("  Feature " .. feature .. " plantlike " .. plantlike)
  end

  for index_x = -room_glyphs.size, room_glyphs.size do
    for index_y = -room_glyphs.size, room_glyphs.size do
      local room_value = room_glyphs[index_x][index_y]
      if (room_value == nil) then
        crawl.mpr("Error: room_value is nil at " .. index_x ..
                          ", " .. index_y)
      end

      -- the mirroring makes this kind of crazy
      local x
      local y
      if (mirror == geoelf.rooms.mirror.NONE or
          mirror == geoelf.rooms.mirror.Y) then
        x = center_x + index_x
      elseif (mirror == geoelf.rooms.mirror.X or
              mirror == geoelf.rooms.mirror.XY) then
        x = center_x - index_x
      elseif (mirror == geoelf.rooms.mirror.D or
              mirror == geoelf.rooms.mirror.DY) then
        x = center_x + index_y
      else
        x = center_x - index_y
      end
      if (mirror == geoelf.rooms.mirror.NONE or
          mirror == geoelf.rooms.mirror.X) then
        y = center_y + index_y
      elseif (mirror == geoelf.rooms.mirror.Y or
              mirror == geoelf.rooms.mirror.XY) then
        y = center_y - index_y
      elseif (mirror == geoelf.rooms.mirror.D or
              mirror == geoelf.rooms.mirror.DX) then
        y = center_y + index_x
      else
        y = center_y - index_x
      end

      -- we have to be careful the outline doesn't go off the map
      if (x >= 1 and x < gxm - 1 and
          y >= 1 and y < gym - 1) then
        local old_glyph = e.mapgrd[x][y]
        if (old_glyph == nil) then
          crawl.mpr("Error: Existing room glyph is nil at " .. x .. ", " .. y)
        end
        local new_glyph
        if (room_value == geoelf.rooms.UNCHANGED) then
          new_glyph = old_glyph
        elseif (room_value == geoelf.rooms.ROOM_OPEN) then
          new_glyph = geoelf.glyphs.OPEN_SUBSTITUTIONS[old_glyph]
        elseif (room_value == geoelf.rooms.ROOM_BORDER) then
          new_glyph = geoelf.glyphs.BORDER_SUBSTITUTIONS[old_glyph]
        elseif (room_value == geoelf.rooms.ROOM_OUTLINE) then
          if (force_open_room_edge) then
            new_glyph = geoelf.glyphs.OUTLINE_SUBSTITUTIONS[old_glyph]
          else
            new_glyph = old_glyph
          end
        elseif (room_value == geoelf.rooms.PLANTLIKE) then
          new_glyph = plantlike
        elseif (room_value == geoelf.rooms.FEATURE) then
          new_glyph = feature
        elseif (room_value == geoelf.rooms.GLASS_PLANTLIKE) then
          new_glyph = geoelf.glyphs.TO_GLASS[plantlike]
        elseif (room_value == geoelf.rooms.GLASS_FEATURE) then
          new_glyph = geoelf.glyphs.TO_GLASS[feature]
        else
          new_glyph = room_value
        end
        if (new_glyph == nil) then
          crawl.mpr("Error: New room glyph is nil at " .. x .. ", " .. y ..
                    " room_value = " .. room_value .. ", old_glyph = " ..
                    old_glyph)
        end
        e.mapgrd[x][y] = new_glyph
      end
    end
  end
end

function geoelf.rooms.choose_shape (room_data, corridor_data, index)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (index >= room_data.count) then
    crawl.mpr("Error: index is past the end of room_data")
  end
  if (room_data[index] == nil) then
    crawl.mpr("Error: Room index does not exist")
  end

  if (geoelf.debug) then
    local corridor_string = ""
    for d = 0, geoelf.directions.COUNT - 1 do
      if (room_data[index].corridor[d] ~= nil) then
        local corridor_index = room_data[index].corridor[d]
        local status = corridor_data[corridor_index].status
        if (status == geoelf.corridors.ACTIVE) then
          corridor_string = corridor_string .. " T" .. status
        else
          corridor_string = corridor_string .. " F" .. status
        end
      else
        corridor_string = corridor_string .. " F "
      end
    end
    print("    Choosing room shape: corridors are" .. corridor_string)
  end
  local shape_is_possible = {}
  for s = 0, geoelf.rooms.shape.COUNT - 1 do
    shape_is_possible[s] = true
    for d = 0, geoelf.directions.COUNT - 1 do
      if (not geoelf.rooms.shape.CONNECTIONS[s][d] and
          room_data[index].corridor[d] ~= nil) then
        local corridor_index = room_data[index].corridor[d]
        local status = corridor_data[corridor_index].status
        if (status == geoelf.corridors.ACTIVE) then
          shape_is_possible[s] = false
          break
        end
      end
    end
  end

  -- OCTAGON always works
  local shape
  local valid_count_so_far = 0
  for s = 0, geoelf.rooms.shape.COUNT - 1 do
    if (shape_is_possible[s]) then
      local frequency = geoelf.rooms.shape.BASE_FREQUENCY[s]
      valid_count_so_far = valid_count_so_far + frequency
      if (crawl.x_chance_in_y(frequency, valid_count_so_far)) then
        shape = s
      end
    end
  end

  return shape
end



----------------------------------------------------------------
--
-- Functions to draw rooms of specific shapes
--
-- There are also supposrt functions to make fancy rooms. These
--  are not subvaults because the layout may need to modify the
--  rooms later.
--
-- The room array always looks something like this:
--
--  112222211
--  122333221                  1: geoelf.rooms.UNCHANGED
--  2233.3322  -2              2: geoelf.rooms.ROOM_OUTLINE
--  233...332                  3: geoelf.rooms.ROOM_BORDER
--  23.....32   0  y index
--  233...332                  .: room contents
--  2233.3322   2                 mostly geoelf.rooms.ROOM_OPEN
--  122333221
--  112222211                  Example shown is a diamond with radius 2
--
--   -2 0 2
--   x index
--
-- The room is always a square centered on [0][0]. The room contents
--  are always bordered with ROOM_OUTLINE (including diagonals),
--  which is always bordered with ROOM_OUTLINE. The extra space
--  is padded with UNCHANGED.
--
-- No room actually uses the outside corners. The square and
--  triangle would, but have been reduced in size. In both
--  cases, this is desirable for other reasons.
--

----------------
--
-- Square rooms
--
-- We cut one row off each side for two reasons:
--  reasons:
--  1. The square should not be bigger than the other room shapes
--  2. The outside corners of the room-defined square often
--     overlap other rooms, and this cuts down on collisions
--

function geoelf.rooms.square (radius, fancy)
  local room = {}
  room.size = radius + 2
  if (geoelf.debug) then
    print("    Drawing square room: radius " .. radius)
  end

  -- make the basic room
  for x = -room.size, room.size do
    local abs_x = math.abs(x)
    room[x] = {}
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      if (abs_x < radius and abs_y < radius) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (abs_x < radius + 1 and abs_y < radius + 1) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (abs_x < radius + 2 and abs_y < radius + 2) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(19)
    if (style < 3) then
      room[0][0] = geoelf.rooms.PLANTLIKE
    elseif (style < 6) then
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 7) then
      -- using function for octagon rooms to avoid code duplication
      geoelf.rooms.octagon_8(room, radius - 1, (radius - 1) * 2,
                             geoelf.rooms.PLANTLIKE)
    elseif (style < 8) then
      geoelf.rooms.octagon_8(room, radius - 1, (radius - 1) * 2,
                             geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 9) then
      geoelf.rooms.octagon_8(room, radius - 1, (radius - 1) * 2,
                             geoelf.rooms.FEATURE)
    elseif (style < 11) then
      geoelf.rooms.square_block(room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 12) then
      geoelf.rooms.square_block(room, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.glyphs.STATUE
    elseif (style < 14) then
      geoelf.rooms.square_lines(room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 15) then
      geoelf.rooms.square_4_corners(room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 16) then
      geoelf.rooms.square_4_corners(room, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 18) then
      -- using function for square rooms to avoid code duplication
      geoelf.rooms.cross_glass_X(room, radius, radius - 1)
    else
      geoelf.rooms.square_glass_box(room, radius)
    end
  end

  return room
end

function geoelf.rooms.square_block (room, radius, glyph)
  local inner_radius = crawl.random_range(1, radius - 2)

  if (geoelf.debug) then
    print("      Adding a block of plantlikes:")
    print("      radius " .. radius .. " inner radius " .. inner_radius)
  end

  for x = -inner_radius, inner_radius do
    for y = -inner_radius, inner_radius do
      room[x][y] = glyph
    end
  end
end

function geoelf.rooms.square_lines (room, radius, glyph)
  local inner_radius = radius - 2
  for x = -inner_radius, inner_radius do
    for y = -inner_radius, inner_radius, 2 do
      room[x][y] = glyph
    end
  end
end

function geoelf.rooms.square_4_corners (room, radius, glyph)
  local inner_radius = radius - 2
  for x = 1, inner_radius do
    for y = 1, inner_radius do
      room[ x][ y] = glyph
      room[ x][-y] = glyph
      room[-x][ y] = glyph
      room[-x][-y] = glyph
    end
  end
end

function geoelf.rooms.square_glass_box (room, radius)
  local inner_radius = crawl.random_range(1, radius - 2)
  for i = 0, inner_radius - 1 do
    room[ i][ inner_radius] = geoelf.glyphs.GLASS
    room[-i][ inner_radius] = geoelf.glyphs.GLASS
    room[ i][-inner_radius] = geoelf.glyphs.GLASS
    room[-i][-inner_radius] = geoelf.glyphs.GLASS
    room[ inner_radius][ i] = geoelf.glyphs.GLASS
    room[ inner_radius][-i] = geoelf.glyphs.GLASS
    room[-inner_radius][ i] = geoelf.glyphs.GLASS
    room[-inner_radius][-i] = geoelf.glyphs.GLASS
  end

  local corner = geoelf.glyphs.GLASS
  local corner_style = crawl.random2(5)
  if (corner_style == 0) then
    corner = geoelf.rooms.GLASS_PLANTLIKE
  elseif (corner_style == 1) then
    corner = geoelf.rooms.GLASS_FEATURE
  end
  room[ inner_radius][ inner_radius] = corner
  room[ inner_radius][-inner_radius] = corner
  room[-inner_radius][ inner_radius] = corner
  room[-inner_radius][-inner_radius] = corner

  local door_pos = crawl.random_range(1 - inner_radius, inner_radius - 1)
  room[door_pos][ inner_radius] = geoelf.glyphs.GLASS_DOOR
  if (crawl.random2(12) < inner_radius) then
    door_pos = crawl.random_range(1 - inner_radius, inner_radius - 1)
    room[door_pos][-inner_radius] = geoelf.glyphs.GLASS_DOOR
  end
  if (crawl.random2(15) < inner_radius) then
    door_pos = crawl.random_range(1 - inner_radius, inner_radius - 1)
    room[ inner_radius][door_pos] = geoelf.glyphs.GLASS_DOOR
  end
end



----------------
--
-- Diamond rooms
--

function geoelf.rooms.diamond (radius, fancy)
  local room = {}
  room.size = radius + 2
  if (geoelf.debug) then
    print("    Drawing diamond room: radius " .. radius)
  end

  -- make the basic room
  for x = -room.size, room.size do
    local abs_x = math.abs(x)
    room[x] = {}
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      local diagonal = abs_x + abs_y
      if (diagonal <= radius) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (diagonal <= radius + 2
              and abs_x <= radius + 1 and abs_y <= radius + 1) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (diagonal <= radius + 4) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(16)
    if (style < 3) then
      room[0][0] = geoelf.rooms.PLANTLIKE
    elseif (style < 6) then
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 7) then
      -- using a function for octagon rooms to avoid code duplication
      geoelf.rooms.octagon_8(room, radius, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 8) then
      geoelf.rooms.octagon_8(room, radius, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 9) then
      geoelf.rooms.octagon_8(room, radius, radius, geoelf.rooms.FEATURE)
    elseif (style < 10) then
      geoelf.rooms.diamond_block(room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 12) then
      geoelf.rooms.diamond_block(room, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.glyphs.STATUE
    elseif (style < 14) then
      geoelf.rooms.diamond_inner_diamond(room, radius, geoelf.rooms.PLANTLIKE)
    else
      geoelf.rooms.diamond_glass_plus(room, radius)
    end
  end

  return room
end

function geoelf.rooms.diamond_inner_diamond (room, radius, glyph)
  local inner_radius = crawl.random_range(1, radius - 2)

  if (geoelf.debug) then
    print("      Adding an inner diamond of plantlikes:")
    print("      radius " .. radius .. " inner radius " .. inner_radius)
  end

  for x = 0, inner_radius do
    local y = inner_radius - x
    room[ x][ y] = glyph
    room[ x][-y] = glyph
    room[-x][ y] = glyph
    room[-x][-y] = glyph
  end
end

function geoelf.rooms.diamond_block (room, radius, glyph)
  local inner_radius = crawl.random_range(1, radius - 2)

  if (geoelf.debug) then
    print("      Adding a block of plantlikes:")
    print("      radius " .. radius .. " inner radius " .. inner_radius)
  end

  for x = -inner_radius, inner_radius do
    for y = -inner_radius, inner_radius do
      local diagonal = math.abs(x) + math.abs(y)
      if (diagonal <= inner_radius) then
        room[x][y] = glyph
      end
    end
  end
end

function geoelf.rooms.diamond_glass_plus (room, radius)
  local both = crawl.coinflip()
  for i = 1, radius do
    room[ i][0] = geoelf.glyphs.GLASS
    room[-i][0] = geoelf.glyphs.GLASS
    if (both) then
      room[0][-i] = geoelf.glyphs.GLASS
      room[0][ i] = geoelf.glyphs.GLASS
    end
  end

  local style = crawl.random2(5)
  if (both and style < 2) then
    if (style == 0) then
      room[0][0] = geoelf.rooms.GLASS_PLANTLIKE
    else
      room[0][0] = geoelf.rooms.GLASS_FEATURE
    end
  else
    room[0][0] = geoelf.glyphs.GLASS
  end

  if (both) then
    local style = crawl.random2(8)
    local door_x1 = crawl.random_range(1, radius - 1)
    local door_x2 = crawl.random_range(1, radius - 1)
    local door_y1 = crawl.random_range(1, radius - 1)
    local door_y2 = crawl.random_range(1, radius - 1)
    if (style ~= 0) then room[ door_x1][0] = geoelf.glyphs.GLASS_DOOR end
    if (style ~= 1) then room[-door_x2][0] = geoelf.glyphs.GLASS_DOOR end
    if (style ~= 2) then room[0][ door_y1] = geoelf.glyphs.GLASS_DOOR end
    if (style ~= 3) then room[0][-door_y2] = geoelf.glyphs.GLASS_DOOR end
  else
    local door_x = crawl.random_range(1 - radius, radius - 1)
    room[door_x][0] = geoelf.glyphs.GLASS_DOOR
  end
end



----------------
--
-- Cross rooms
--

function geoelf.rooms.cross (radius, indent, fancy)
  local room = {}
  room.size = radius + 2
  local center_radius = radius - indent

  -- make the basic room
  for x = -room.size, room.size do
    local abs_x = math.abs(x)
    room[x] = {}
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      local smaller_abs = math.min(abs_x, abs_y)
      if (abs_x <= radius and
          abs_y <= radius and
          smaller_abs <= center_radius) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (abs_x <= radius + 1 and
              abs_y <= radius + 1 and
              smaller_abs <= center_radius + 1) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (smaller_abs <= center_radius + 2) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(18)

    if (style < 3) then
      room[0][0] = geoelf.rooms.PLANTLIKE
    elseif (style < 6) then
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 7) then
      geoelf.rooms.cross_4(room, radius, center_radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 9) then
      geoelf.rooms.cross_4(room, radius, center_radius, geoelf.rooms.FEATURE)
    elseif (style < 10) then
      geoelf.rooms.cross_block(room, radius, center_radius,
                               geoelf.rooms.PLANTLIKE)
    elseif (style < 12) then
      geoelf.rooms.cross_block(room, radius, center_radius,
                               geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.glyphs.STATUE
    elseif (style < 14) then
      geoelf.rooms.cross_glass_X(room, radius, center_radius)
    elseif (style < 16) then
      if (indent > 2) then
        geoelf.rooms.cross_glass_5_parts(room, radius, center_radius)
      end
    else
      geoelf.rooms.cross_glass_plus(room, radius, center_radius)
    end
  end

  return room
end

function geoelf.rooms.cross_4 (room, radius, center_radius, glyph)
  local pos = crawl.random_range(1, radius - 1)
  room[pos][0]  = glyph
  room[-pos][0] = glyph
  room[0][pos]  = glyph
  room[0][-pos] = glyph
end

function geoelf.rooms.cross_block (room, radius, center_radius, glyph)
  local arm_width  = crawl.random2(center_radius)
  local arm_length = crawl.random_range(arm_width + 1, radius - 1)

  for x = -arm_length, arm_length do
    local abs_x = math.abs(x)
    for y = -arm_length, arm_length do
      local abs_y = math.abs(y)
      local smaller_abs = math.min(abs_x, abs_y)
      if (abs_x <= arm_length and
          abs_y <= arm_length and
          smaller_abs <= arm_width) then
        room[x][y] = glyph
      end
    end
  end
end

function geoelf.rooms.cross_glass_X (room, radius, center_radius)
  local both = crawl.coinflip()
  for i = 1, center_radius do
    room[ i][ i] = geoelf.glyphs.GLASS
    room[-i][-i] = geoelf.glyphs.GLASS
    if (both) then
      room[ i][-i] = geoelf.glyphs.GLASS
      room[-i][ i] = geoelf.glyphs.GLASS
    end
  end

  local style = crawl.random2(5)
  if (both and style < 2) then
    if (style == 0) then
      room[0][0] = geoelf.rooms.GLASS_PLANTLIKE
    else
      room[0][0] = geoelf.rooms.GLASS_FEATURE
    end
  else
    room[0][0] = geoelf.glyphs.GLASS
  end
end

function geoelf.rooms.cross_glass_5_parts (room, radius, center_radius)
  local wall_min = -center_radius - 1
  local wall_max =  center_radius + 1
  for i = -center_radius, center_radius do
    room[i][wall_min] = geoelf.glyphs.GLASS
    room[i][wall_max] = geoelf.glyphs.GLASS
    room[wall_min][i] = geoelf.glyphs.GLASS
    room[wall_max][i] = geoelf.glyphs.GLASS
  end

  local door_min = -center_radius + 1
  local door_max =  center_radius - 1
  local door_x1 = crawl.random_range(door_min, door_max)
  local door_x2 = crawl.random_range(door_min, door_max)
  local door_y1 = crawl.random_range(door_min, door_max)
  local door_y2 = crawl.random_range(door_min, door_max)
  room[door_x1][wall_min] = geoelf.glyphs.GLASS_DOOR
  room[door_x2][wall_max] = geoelf.glyphs.GLASS_DOOR
  room[wall_min][door_y1] = geoelf.glyphs.GLASS_DOOR
  room[wall_max][door_y2] = geoelf.glyphs.GLASS_DOOR
end

function geoelf.rooms.cross_glass_plus (room, radius, center_radius)
  local arm_length = crawl.random_range(1, radius - 1)
  for i = 1, arm_length - 1 do
    room[ i][0] = geoelf.glyphs.GLASS
    room[-i][0] = geoelf.glyphs.GLASS
    room[0][ i] = geoelf.glyphs.GLASS
    room[0][-i] = geoelf.glyphs.GLASS
  end

  local style   = crawl.random2(10)
  local center  = geoelf.glyphs.GLASS
  local arm_end = geoelf.glyphs.GLASS
  if (style == 0) then
    center = geoelf.rooms.GLASS_PLANTLIKE
  elseif (style == 1) then
    center = geoelf.rooms.GLASS_FEATURE
  elseif (style == 2) then
    arm_end = geoelf.rooms.GLASS_PLANTLIKE
  elseif (style == 3) then
    arm_end = geoelf.rooms.GLASS_FEATURE
  end
  room[0][0]           = center
  room[ arm_length][0] = arm_end
  room[-arm_length][0] = arm_end
  room[0][ arm_length] = arm_end
  room[0][-arm_length] = arm_end
end



----------------
--
-- Triangle rooms
--
-- The apex of the triangle points east (X+)
--
-- We cut one row off the base (and its border cells) for two
--  reasons:
--  1. The triangle looks better (with the full base, it look
--     off-center because one side is bigger)
--  2. The outside corners of the room-defined square often
--     overlap other rooms, and this cuts down on collisions
--

function geoelf.rooms.triangle (radius, fancy)
  local room = {}
  room.size = radius + 2
  if (geoelf.debug) then
    print("    Drawing triangle room: radius " .. radius)
  end

  -- make the basic room
  for x = -room.size, room.size do
    room[x] = {}
    local max_y = (radius + 1 - x) / 2
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      if (x > -radius and
          abs_y < max_y) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (x > -radius - 1 and
              x <= radius + 1 and
              abs_y <= max_y + 1 and
              abs_y <= radius) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (x > -radius - 2 and
              abs_y < max_y + 3 and
              abs_y <= radius + 1) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(15)
    local twice_border = crawl.random_range(3, radius)
    if (style < 4) then
      geoelf.rooms.triangle_singles(room, radius, twice_border,
                                    geoelf.rooms.PLANTLIKE, false)
    elseif (style < 7) then
      geoelf.rooms.triangle_singles(room, radius, twice_border,
                                    geoelf.rooms.FEATURE, false)
    elseif (style < 8) then
      geoelf.rooms.triangle_block(room, radius, twice_border,
                                  geoelf.rooms.PLANTLIKE)
    elseif (style < 10) then
      geoelf.rooms.triangle_block(room, radius, twice_border,
                                  geoelf.rooms.PLANTLIKE)
      geoelf.rooms.triangle_singles(room, radius, twice_border,
                                    geoelf.rooms.FEATURE, true)
    elseif (style < 13) then
      geoelf.rooms.triangle_lines(room, radius,
                                  geoelf.rooms.PLANTLIKE)
    else
      geoelf.rooms.triangle_glass_wall(room, radius)
    end
  end

  return room
end

function geoelf.rooms.triangle_singles (room, radius, twice_border, glyph,
                                        force_center_feature_to_statue)
  local min_x = -radius + math.floor(twice_border / 2) + 1
  local room_max_y = math.ceil((radius + 1 - twice_border - min_x) / 2) - 1
  if (geoelf.debug) then
    print("      Adding single glyphs: radius " .. radius ..
          " twice_border " .. twice_border .. " min_x " .. min_x ..
          " room_max_y " .. room_max_y)
  end

  local style = crawl.random2(8)
  if (radius <= 3) then
    style = 0
  end
  if (style <= 3) then
    if (force_center_feature_to_statue) then
      glyph = geoelf.glyphs.STATUE
    end
    room[-1][0] = glyph
  elseif (style == 4) then
    room[radius - twice_border][0] = glyph
  elseif (style == 5) then
    room[min_x][ room_max_y] = glyph
    room[min_x][-room_max_y] = glyph
  elseif (style == 6) then
    room[radius - twice_border][0] = glyph
    room[min_x][ room_max_y] = glyph
    room[min_x][-room_max_y] = glyph
  else
    room[min_x][0] = glyph
  end
end

function geoelf.rooms.triangle_block (room, radius, twice_border, glyph)
  local min_x = -radius + math.floor(twice_border / 2) + 1
  if (geoelf.debug) then
    print("      Adding a block of glyphs: radius " .. radius ..
          " twice_border " .. twice_border .. " min_x " .. min_x)
  end

  for x = min_x, radius - twice_border do
    local max_y = math.ceil((radius + 1 - twice_border - x) / 2 - 1)
    for y = -max_y, max_y do
      local abs_y = math.abs(y)
      if (abs_y <= max_y) then
        room[x][y] = glyph
      end
    end
  end
end

function geoelf.rooms.triangle_lines (room, radius, glyph)
  local min_x = -radius + crawl.random_range(2, 3)
  if (geoelf.debug) then
    print("      Adding a lines of glyphs: radius " .. radius ..
          " min_x " .. min_x)
  end

  -- note that this loop goes up by 2
  for x = min_x, radius - 3, 2 do
    local max_y = math.ceil((radius - 2 - x) / 2 - 1)
    for y = -max_y, max_y do
      local abs_y = math.abs(y)
      if (abs_y <= max_y) then
        room[x][y] = glyph
      end
    end
  end
end

function geoelf.rooms.triangle_glass_wall (room, radius)
  for x = -radius + 1, radius do
    room[x][0] = geoelf.glyphs.GLASS
  end
  local door_x = crawl.random_range(-radius + 2, radius - 2)
  room[door_x][0] = geoelf.glyphs.GLASS_DOOR
end



----------------
--
-- Hexagon rooms
--
-- The flat sides of the hexagon face east and west (X+, X-)
--

function geoelf.rooms.hexagon (radius, fancy)
  local room = {}
  room.size = radius + 2
  local x_addend = radius % 2

  -- make the basic room
  for x = -room.size, room.size do
    room[x] = {}
    local abs_x = math.abs(x)
    local max_y    = radius - math.ceil((abs_x - x_addend)     / 2)
    local border_y = radius - math.ceil((abs_x - x_addend - 3) / 2)
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      if (abs_x <= radius and
          abs_y <= max_y) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (abs_x <= radius + 1 and
              abs_y <= radius + 1 and
              abs_y <= border_y) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (abs_y <= max_y + 3) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(17)
    if (radius <= 3) then
      style = crawl.random2(6)
    end

    if (style < 3) then
      room[0][0] = geoelf.rooms.PLANTLIKE
    elseif (style < 6) then
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 7) then
      geoelf.rooms.hexagon_singles(room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 9) then
      geoelf.rooms.hexagon_singles(room, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 11) then
      geoelf.rooms.hexagon_singles(room, radius, geoelf.rooms.FEATURE)
    elseif (style < 12) then
      geoelf.rooms.hexagon_block (room, radius, geoelf.rooms.PLANTLIKE)
    elseif (style < 14) then
      geoelf.rooms.hexagon_block (room, radius, geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.glyphs.STATUE
    elseif (style < 15) then
      geoelf.rooms.hexagon_glass_wall_S_N (room, radius)
    else
      -- this one is at twice the weight of the straight
      --  one because there are two diagonal directions
      geoelf.rooms.hexagon_glass_wall_SE_NW (room, radius)
    end
  end

  return room
end

function geoelf.rooms.hexagon_singles (room, radius, glyph)
  local style = crawl.random2(10)
  if (style <= 4) then
    geoelf.rooms.hexagon_3(room, radius, glyph)
  elseif (style <= 7) then
    geoelf.rooms.hexagon_6(room, radius, glyph, geoelf.rooms.ROOM_OPEN)
  else
    geoelf.rooms.hexagon_6(room, radius, glyph, glyph)
  end
end

function geoelf.rooms.hexagon_3 (room, radius, glyph)
  local distance = crawl.random_range(2, radius - 1)
  local x = -math.floor(distance / 2)
  local y =  math.floor(distance * 3/4)

  room[distance][0] = glyph
  room[x][ y]       = glyph
  room[x][-y]       = glyph
end

function geoelf.rooms.hexagon_6 (room, radius, glyph1, glyph2)
  local distance = crawl.random_range(2, radius - 1)
  local y_small = math.floor(distance / 2)
  local y_big = distance
  if (distance == radius - 1 and radius % 2 == 0) then
    -- special case, move statues out of corners
    y_big = y_big - 1
  end

  room[ 0       ][ y_big]   = glyph1
  room[ distance][ y_small] = glyph2
  room[ distance][-y_small] = glyph1
  room[ 0       ][-y_big]   = glyph2
  room[-distance][-y_small] = glyph1
  room[-distance][ y_small] = glyph2
end

function geoelf.rooms.hexagon_block (room, radius, glyph)
  local inner_radius = math.min(crawl.random_range(1, radius - 2), radius - 2)
  local x_addend = inner_radius % 2

  for x = -inner_radius, inner_radius do
    local max_y = inner_radius - math.ceil((math.abs(x) - x_addend) / 2)
    for y = -inner_radius, inner_radius do
      local abs_y = math.abs(y)
      if (abs_y <= max_y) then
        room[x][y] = glyph
      end
    end
  end
end

function geoelf.rooms.hexagon_glass_wall_S_N (room, radius)
  for y = -radius, radius do
    room[0][y] = geoelf.glyphs.GLASS
  end
  local door_y = crawl.random_range(-radius + 1, radius - 1)
  room[0][door_y] = geoelf.glyphs.GLASS_DOOR
end

function geoelf.rooms.hexagon_glass_wall_SE_NW (room, radius)
  for x = 1, radius do
    local y = math.floor(x / 2)
    room[ x][ y] = geoelf.glyphs.GLASS
    room[-x][-y] = geoelf.glyphs.GLASS
  end

  -- We don't actually need a door because the wall is diagonal
  -- If the door isn't in the middle, it will be removed due
  --  to having insufficient walls around it
  if (crawl.one_chance_in(3)) then
    room[0][0] = geoelf.glyphs.GLASS_DOOR
  else
    room[0][0] = geoelf.glyphs.GLASS
  end
end



----------------
--
-- Octagon rooms
--

function geoelf.rooms.octagon (radius, diagonal_cutoff, fancy)
  local room = {}
  room.size = radius + 2
  if (geoelf.debug) then
    print("    Drawing octagon room: radius " .. radius ..
          " diagonal_cutoff " .. diagonal_cutoff)
  end

  -- make the basic room
  for x = -room.size, room.size do
    local abs_x = math.abs(x)
    room[x] = {}
    for y = -room.size, room.size do
      local abs_y = math.abs(y)
      local diagonal = abs_x + abs_y
      if (abs_x <= radius and
          abs_y <= radius and
          diagonal <= diagonal_cutoff) then
        room[x][y] = geoelf.rooms.ROOM_OPEN
      elseif (abs_x <= radius + 1 and
              abs_y <= radius + 1 and
              diagonal <= diagonal_cutoff + 2) then
        room[x][y] = geoelf.rooms.ROOM_BORDER
      elseif (diagonal <= diagonal_cutoff + 4) then
        room[x][y] = geoelf.rooms.ROOM_OUTLINE
      else
        room[x][y] = geoelf.rooms.UNCHANGED
      end
    end
  end

  -- apply fancy stuff if requested
  if (fancy and radius > 2) then
    local style = crawl.random2(14)
    if (style < 3) then
      room[0][0] = geoelf.rooms.PLANTLIKE
    elseif (style < 6) then
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 8) then
      geoelf.rooms.octagon_8(room, radius, diagonal_cutoff,
                             geoelf.rooms.PLANTLIKE)
    elseif (style < 10) then
      geoelf.rooms.octagon_8(room, radius, diagonal_cutoff,
                             geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.rooms.FEATURE
    elseif (style < 11) then
      geoelf.rooms.octagon_8(room, radius, diagonal_cutoff,
                             geoelf.rooms.FEATURE)
    elseif (style < 12) then
      geoelf.rooms.octagon_block(room, radius, diagonal_cutoff,
                                 geoelf.rooms.PLANTLIKE)
    else
      geoelf.rooms.octagon_block(room, radius, diagonal_cutoff,
                                 geoelf.rooms.PLANTLIKE)
      room[0][0] = geoelf.glyphs.STATUE
    end
  end

  return room
end

function geoelf.rooms.octagon_8 (room, radius, diagonal_cutoff, glyph)
  local pos1 = crawl.random_range(0, radius - 1)
  local pos2_max = math.min(pos1, diagonal_cutoff - pos1 - 2)
  local pos2 = crawl.random2(pos2_max + 1)

  room[ pos1][ pos2] = glyph
  room[ pos1][-pos2] = glyph
  room[-pos1][ pos2] = glyph
  room[-pos1][-pos2] = glyph
  room[ pos2][ pos1] = glyph
  room[ pos2][-pos1] = glyph
  room[-pos2][ pos1] = glyph
  room[-pos2][-pos1] = glyph
end

function geoelf.rooms.octagon_block (room, radius, diagonal_cutoff, glyph)
  local radius_reduction = crawl.random_range(1, radius - 1)
  local inner_radius = radius - radius_reduction
  local raw_cutoff = crawl.random_range(inner_radius, inner_radius * 2)
  local inner_diagonal_cutoff = math.min(diagonal_cutoff - 2, raw_cutoff)

  if (geoelf.debug) then
    print("      Adding a block of plantlikes:")
    print("      radius " .. radius .. " reduction " .. radius_reduction ..
          " = inner radius " .. inner_radius .. " inner diagonal cutoff " ..
          inner_diagonal_cutoff)
  end

  for x = -inner_radius, inner_radius do
    for y = -inner_radius, inner_radius do
      local diagonal = math.abs(x) + math.abs(y)
      if (diagonal <= inner_diagonal_cutoff) then
        room[x][y] = glyph
      end
    end
  end
end
