------------------------------------------------------------------------------
-- geoelf.lua:
--
-- Functions that draw the corridors for the geoelf layout.
------------------------------------------------------------------------------

geoelf.corridors = {}    -- Namespace for corridor drawing functions

crawl_require("dlua/layout/geoelf_directions.lua")
crawl_require("dlua/layout/geoelf_glyphs.lua")

----------------
--
-- Corridor states
--
-- These are the possible states for a possible corridor.
--

geoelf.corridors.INACTIVE = 0
geoelf.corridors.ACTIVE   = 1
geoelf.corridors.BLOCKED  = 2

----------------------------------------------------------------
--
-- Drawing corridors
--


function geoelf.corridors.draw (e, room_data, corridor_data, index)
  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end
  if (index >= corridor_data.count) then
    crawl.mpr("Error: index is past the end of corridor_data")
  end
  if (corridor_data[index].status ~= geoelf.corridors.ACTIVE ) then
    crawl.mpr("Error: Attempting to draw an invalid corridor type")
  end

  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  local room1_index = corridor_data[index].room1
  local room2_index = corridor_data[index].room2
  if (room1_index >= room_data.count or
      room2_index >= room_data.count) then
    crawl.mpr("Error: Index of room is past the end of room_data")
  end
  if (room_data[room1_index] == nil or
      room_data[room2_index] == nil) then
    crawl.mpr("Error: Room referenced by corridor does not exist")
  end
  local direction = corridor_data[index].direction

  -- flip "backwards" corridors
  if (geoelf.directions.IS_REVERSE[direction]) then
    direction = geoelf.directions.GET_REVERSE[direction]
    local temp  = room1_index
    room1_index = room2_index
    room2_index = temp
  end

  -- I don't think we need fancier corridors or even wider ones
  if (direction == geoelf.directions.S) then
    geoelf.corridors.draw_s (e, room_data, room1_index, room2_index)
  elseif (direction == geoelf.directions.E) then
    geoelf.corridors.draw_e (e, room_data, room1_index, room2_index)
  elseif (direction == geoelf.directions.SE) then
    geoelf.corridors.draw_se(e, room_data, room1_index, room2_index)
  elseif (direction == geoelf.directions.SW) then
    geoelf.corridors.draw_sw(e, room_data, room1_index, room2_index)
  else
    crawl.mpr("Error: Trying to draw corridor with invalid direction " ..
              direction)
  end
end

function geoelf.corridors.draw_s (e, room_data, room1_index, room2_index)
  local x1 = room_data[room1_index].center_x
  local x2 = room_data[room2_index].center_x
  local radius1 = room_data[room1_index].radius
  local radius2 = room_data[room2_index].radius
  local x = geoelf.corridors.calculate_position(x1,      x2,
                                                radius1, radius2)

  local start_y = room_data[room1_index].center_y
  local end_y   = room_data[room2_index].center_y
  if (start_y > end_y) then
    crawl.mpr("Error: Corridor S is being drawn backwards")
  end

  --print("Drawing corridor S from y = " .. start_y .. " to " .. end_y ..
  --      " x = " .. x)
  for y = start_y, end_y do
    e.mapgrd[x][y] = geoelf.glyphs.CORRIDOR
  end
end

function geoelf.corridors.draw_e (e, room_data, room1_index, room2_index)
  local y1 = room_data[room1_index].center_y
  local y2 = room_data[room2_index].center_y
  local radius1 = room_data[room1_index].radius
  local radius2 = room_data[room2_index].radius
  local y = geoelf.corridors.calculate_position(y1,      y2,
                                                radius1, radius2)

  local start_x = room_data[room1_index].center_x
  local end_x   = room_data[room2_index].center_x
  if (start_x > end_x) then
    crawl.mpr("Error: Corridor E is being drawn backwards")
  end

  --print("Drawing corridor E from x = " .. start_x .. " to " .. end_x ..
  --      " y = " .. y)
  for x = start_x, end_x do
    e.mapgrd[x][y] = geoelf.glyphs.CORRIDOR
  end
end

function geoelf.corridors.draw_se (e, room_data, room1_index, room2_index)
  local x_minus_y1 = room_data[room1_index].center_x -
                     room_data[room1_index].center_y
  local x_minus_y2 = room_data[room2_index].center_x -
                     room_data[room2_index].center_y
  local radius1 = math.floor(room_data[room1_index].radius * 3/4)
  local radius2 = math.floor(room_data[room2_index].radius * 3/4)
  local x_minus_y = geoelf.corridors.calculate_position(x_minus_y1, x_minus_y2,
                                                        radius1,    radius2)

  local start_y = room_data[room1_index].center_y
  local end_y   = room_data[room2_index].center_y
  if (start_y > end_y) then
    crawl.mpr("Error: Corridor SE is being drawn backwards")
  end

  --print("Drawing corridor SE from y = " .. start_y .. " to " .. end_y ..
  --      " x_minus_y = " .. x_minus_y)
  for y = start_y, end_y do
    e.mapgrd[x_minus_y + y][y] = geoelf.glyphs.CORRIDOR
  end
end

function geoelf.corridors.draw_sw (e, room_data, room1_index, room2_index)
  local x_plus_y1 = room_data[room1_index].center_x +
                    room_data[room1_index].center_y
  local x_plus_y2 = room_data[room2_index].center_x +
                    room_data[room2_index].center_y
  local radius1 = math.floor(room_data[room1_index].radius * 3/4)
  local radius2 = math.floor(room_data[room2_index].radius * 3/4)
  local x_plus_y = geoelf.corridors.calculate_position(x_plus_y1, x_plus_y2,
                                                       radius1,   radius2)

  local start_y = room_data[room1_index].center_y
  local end_y   = room_data[room2_index].center_y
  if (start_y > end_y) then
    crawl.mpr("Error: Corridor SW is being drawn backwards")
  end

  --print("Drawing corridor SW from y = " .. start_y .. " to " .. end_y ..
  --      " x_plus_y = " .. x_plus_y)
  for y = start_y, end_y do
    e.mapgrd[x_plus_y - y][y] = geoelf.glyphs.CORRIDOR
  end
end

function geoelf.corridors.calculate_position (pos1, pos2, radius1, radius2)
  local pos_min
  local pos_max
  if (pos1 < pos2) then
    pos_min = math.max(pos1, pos2 - radius2)
    pos_max = math.min(pos2, pos1 + radius1)
  else
    pos_min = math.max(pos2, pos1 - radius1)
    pos_max = math.min(pos1, pos2 + radius2)
  end

  if (pos_min < pos_max) then
    -- anything in this range is good
    return crawl.random_range(pos_min, pos_max)
  else
    -- if nothing is good, use the middle of the less bad range
    return math.floor((pos_min + pos_max) / 2)
  end
end
