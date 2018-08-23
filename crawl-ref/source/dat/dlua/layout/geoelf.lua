------------------------------------------------------------------------------
-- geoelf.lua: The layout engine that generates layouts of
--             geometric rooms for the Elf branch
--
-- Main include for Geoelf layouts. This file (directly)
--  contains the functions for specifying rooms and possible
--  corridors, as well as the master function that runs the
--  layout once all the rooms and corridors have been specified.
------------------------------------------------------------------------------

geoelf = {}          -- Main namespace for engine

-- This switch prints what is happening to console and the final
--  map at the end. Only advisable if you start tiles from a
--  command-line.
geoelf.debug = false

crawl_require("dlua/layout/geoelf_directions.lua")
crawl_require("dlua/layout/geoelf_glyphs.lua")
crawl_require("dlua/layout/geoelf_rooms.lua")
crawl_require("dlua/layout/geoelf_corridors.lua")

----------------------------------------------------------------
--
--  How to use the geoelf layout generator:
--
--  1. Initialize the room and corridor arrays in the layout.
--     They will be needed as parameters to the subsequent
--     functions. Use the following code:
--         local room_data     = { count=0 }
--         local corridor_data = { count=0 }
--  2. Specify the rooms to include using the geoelf.add_room
--     function (see below for details).
--  3. Specify the possible corridors between the rooms using
--     the geoelf.add_corridor function (see below for details).
--    -> Not all of these corridors will (normally) appear in
--       the final layout.
--    -> At most 1 corridor can be attached to any given room in
--       any given direction
--    -> These may be interspersed between the rooms as long as
--       both rooms the corridor is connected to have been
--       specified.
--  4. Call the geoelf.generate function (see below for details).
--
--  The geoelf layout defines special meanings for the following
--   glyphs:
--    -> D, E, F: bush, plant, fungus (if enabled)
--    -> -: glass door in glass (currently door with retiled floor,
--                               TODO: tile, code)
--    -> J, K, L, M, N, O: glass with pictures of tree, bush,
--                         plant, fungus, statue, fountain
--                         (needs special tiles)
--
--  If you need to connect the portion of the map generated
--   using geoelf layout generator ("the geoelf layout") to a
--   portion of the map generated in a different way ("your
--   layout"):
--
--  1.-3. As above
--  4. Use the add_room_dummy function to mark where on your
--     layout you want the geoelf layout to connect to
--    -> Nothing will be displayed for these rooms
--  5. Add the possible corridors to connect the dummy rooms to
--     the geoelf layout
--    -> Not all corridors will appear, but at least one will
--       connect to each dummy room
--    -> If you connect the dummy rooms to each other, corridors
--       will be added between them as well
--  6. Call the geoelf.generate function
--



----------------
--
-- add_room
--
-- Purpose: This function adds a room with the specified
--          properties to the list of rooms for the layout.
-- Parameter(s):
--  -> room_data: The array of room information. This new room
--                will be added to the array.
--  -> center_x
--  -> center_y: The x/y coordinates for the center of the room.
--  -> radius: The radius for the room. The exact meaning of
--             the radius varies depending on what shape the
--             room eventually has, but no part of the final
--             room will be farther than this (chessboard
--             distance) from the center.
-- Returns: The index of the room. You will need this to
--          specify possible corridors.
--
-- The room data structure has
--  -> center_x: The base x coordinate of the room center
--  -> center_y: The base y coordinate of the room center
--  -> radius: The size from room center to edge
--  -> zone: used to connect the rooms into a tree
--  -> corridor: An array of the corridors indexed using
--               direction values
--  -> is_dummy: Whether this room is a plaeholder
--
-- The elements in the array of room data are numbered from 0 as
--  in C++, not from 1 as recommended in LUA documentation. The
--  array also has a count field storing the number of elements.
--

function geoelf.add_room (room_data, center_x, center_y, radius)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (center_x == nil) then
    crawl.mpr("Error: center_x is nil")
  end
  if (center_y == nil) then
    crawl.mpr("Error: center_y is nil")
  end
  if (radius == nil) then
    crawl.mpr("Error: radius is nil")
  end

  if (geoelf.debug) then
    print("geoelf.add_room(room_data, " .. center_x .. ", " .. center_y ..
          ", " .. radius .. ")")
  end

  local index = room_data.count
  room_data[index] = {}

  room_data[index].center_x = center_x
  room_data[index].center_y = center_y
  room_data[index].radius   = radius
  room_data[index].zone     = index
  room_data[index].corridor = {}
  room_data[index].is_dummy = false

  for i = 0, geoelf.directions.COUNT - 1 do
    room_data[index].corridor[i] = nil
  end

  room_data.count = index + 1
  return index
end

----------------
--
-- add_room_dummy
--
-- Purpose: This function adds a dummy room at the specified
--          position to the list of rooms for the layout. A
--          dummy room is connected into the tree but is never
--          drawn to the layout. Dummy rooms should be used to
--          connect the geoelf room system to parts of a map
--          generate using a different method.
-- Parameter(s):
--  -> room_data: The array of room information. This new room
--                will be added to the array.
--  -> center_x
--  -> center_y: The x/y coordinates for the center of the room.
-- Returns: The index of the room. You will need this to
--          specify possible corridors.
--
-- The room data structure is the same as for add_room
--

function geoelf.add_room_dummy (room_data, center_x, center_y)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (center_x == nil) then
    crawl.mpr("Error: center_x is nil")
  end
  if (center_y == nil) then
    crawl.mpr("Error: center_y is nil")
  end

  if (geoelf.debug) then
    print("geoelf.add_room_dummy(room_data, " .. center_x .. ", " ..
          center_y .. ")")
  end

  local index = room_data.count
  room_data[index] = {}

  room_data[index].center_x = center_x
  room_data[index].center_y = center_y
  room_data[index].radius   = 1
  room_data[index].zone     = index
  room_data[index].corridor = {}
  room_data[index].is_dummy = true

  for i = 0, geoelf.directions.COUNT - 1 do
    room_data[index].corridor[i] = nil
  end

  room_data.count = index + 1
  return index
end

----------------
--
-- is_room_dummy
--
-- Purpose: To determine if the room with the specified index
--          is a dummy room dummy room.
-- Parameter(s):
--  -> room_data: The array of room information.
--  -> index: The index of the room
-- Returns: Whether the room in room_data with index index is a
--          dummy room.
--

function geoelf.is_room_dummy (room_data, index)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (index == nil) then
    crawl.mpr("Error: index is nil")
  end
  if (index >= room_data.count) then
    crawl.mpr("Error: index is larger than room count")
  end

  return room_data[index].is_dummy
end

----------------
--
-- add_corridor
--
-- Purpose: This function adds a possible corridor to the list
--          for the layout with the specified properties.
-- Parameter(s):
--  -> room_data: The array of room information. The rooms with
--                corridor attaches to will be marked as
--                connected in that direction
--  -> corridor_data: The array of corridor information. The
--                    new corridor will be added to this array
--  -> room1
--  -> room2: The indexes of the two rooms this corridor
--            connects
--  -> direction: The direction of the corridor from room1.
--                There is no sanity check here, so specifying
--                an incorrect direction can produce disconnect
--                layouts.
--  -> block_index: The index of the corridor that would
--                  intersect this one. The block will also
--                  apply in reverse. A corridor can block at
--                  most one other corridor. The layout
--                  generator will not include 2 corridors that
--                  block each other.
-- Returns: The index of the corridor. You can also get this
--          from the room data array.
--
-- The corridor data structure has
--  -> room1: The room at one end
--  -> room2: The room at the other end
--  -> direction: The direction of the corridor from room 1
--  -> status: Whether the corridor is inactive, required
--             active, extra active, or blocked by another
--  -> block: The index of the corridor that this corridor
--            blocks, if any
--
-- The elements in the array of corridor data are numbered from
--  0 as in C++, not from 1 as recommended in LUA documentation.
--  The array also has a count field storing the number of
--  elements.
--

function geoelf.add_corridor (room_data, corridor_data,
                              room1, room2,
                              direction, block_index)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end
  if (room1 == nil) then
    crawl.mpr("Error: room1 is nil")
  end
  if (room2 == nil) then
    crawl.mpr("Error: room2 is nil")
  end
  if (direction == nil) then
    crawl.mpr("Error: direction is nil")
  end

  if (geoelf.debug) then
    if (block_index == nil) then
      print("geoelf.add_corridor(room_data, corridor_data, " ..
            room1 .. ", " .. room2 .. ", " .. direction .. ", nil)")
    else
      print("geoelf.add_corridor(room_data, corridor_data, " .. room1 ..
            ", " .. room2 .. ", " .. direction .. ", " .. block_index .. ")")
    end
  end

  local index = corridor_data.count
  corridor_data[index] = {}

  corridor_data[index].room1     = room1
  corridor_data[index].room2     = room2
  corridor_data[index].direction = direction
  corridor_data[index].status    = geoelf.corridors.INACTIVE
  corridor_data[index].block     = block_index

  if (block_index ~= nil) then
    corridor_data[block_index].block = index
  end

  local reverse_direction = geoelf.directions.GET_REVERSE[direction]
  room_data[room1].corridor[direction]         = index
  room_data[room2].corridor[reverse_direction] = index

  corridor_data.count = index + 1
  return index
end

----------------
--
-- generate
--
-- Purpose: This function generates the layout from the rooms
--          and possible corridors that have been specified.
-- Parameter(s):
--  -> e: A reference to the global environment. Pass in _G.
--  -> room_data: The array of room information
--  -> corridor_data: The array of corridor information
--  -> extra_fraction: The fraction of the extra corridors to
--                     include beyond the minimum needed to
--                     connect the map
--  -> fancy_room_fraction: The fraction of the rooms that have
--                          ornamentation in them
--  -> only_trees: Whether all bushes, plants, and fungus should
--                 be converted to trees
--    -> we may not want bushes, plants, fungus
--      -> they interfere with vault placement
--    -> trees do not interfere because they are considered to
--       be a terrain type instead of a monster
--  -> force_open_room_edge: Whether to force overwritten rooms
--                           to have open floor at the new room
--                           edge
--    -> the maps normally look better with this off, but can
--       become disconnected in a few very rare cases
-- Returns: N/A
--

function geoelf.generate (e, room_data, corridor_data,
                          extra_fraction, fancy_room_fraction,
                          only_trees, force_open_room_edge)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end
  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end
  if (extra_fraction == nil) then
    crawl.mpr("Error: extra_fraction is nil")
  end
  if (fancy_room_fraction == nil) then
    crawl.mpr("Error: fancy_room_fraction is nil")
  end
  if (only_trees == nil) then
    crawl.mpr("Error: only_trees is nil")
  end
  if (force_open_room_edge == nil) then
    crawl.mpr("Error: force_open_room_edge is nil")
  end

  if (geoelf.debug) then
    print("geoelf.generate(room_data, corridor_data, " .. extra_fraction .. ")")
  end

  -- choose corridors
  if (geoelf.debug) then print("Choosing corridors:") end
  geoelf.corridor_activate_tree  (room_data, corridor_data)
  geoelf.corridor_activate_random(room_data, corridor_data, extra_fraction)

  -- draw corridors
  if (geoelf.debug) then print("Drawing corridors:") end
  for i = 0, corridor_data.count - 1 do
    if (corridor_data[i].status == geoelf.corridors.ACTIVE) then
      geoelf.corridors.draw(e, room_data, corridor_data, i)
    end
  end

  -- choose a random order to draw rooms
  if (geoelf.debug) then print("Choosing room draw order:") end
  local draw_order = {}
  for i = 0, room_data.count - 1 do
    local other_index = crawl.random2(i + 1)
    draw_order[i] = draw_order[other_index]
    draw_order[other_index] = i
  end

  -- draw rooms
  if (geoelf.debug) then print("Drawing rooms:") end
  for i = 0, room_data.count - 1 do
    if (not room_data[draw_order[i]].is_dummy) then
      geoelf.rooms.draw(e, room_data, corridor_data, draw_order[i],
                        (crawl.random_real() < fancy_room_fraction),
                        force_open_room_edge)
    end
  end

  -- substitutions
  if (geoelf.debug) then print("Epilog:") end
  e.subst(geoelf.glyphs.CORRIDOR .. " = " .. geoelf.glyphs.FLOOR)
  --geoelf.beautify.remove_extra_doors(e)
  e.remove_disconnected_doors {door    = geoelf.glyphs.ALL_DOORS,
                               open    = (geoelf.glyphs.ALL_FLOORLIKE ..
                                          geoelf.glyphs.ALL_DOORS),
                               replace = geoelf.glyphs.FLOOR }
  e.add_windows {wall   = geoelf.glyphs.ALL_WALLLIKE,
                 open   = geoelf.glyphs.ALL_FLOORLIKE,
                 window = geoelf.glyphs.GLASS }
  geoelf.make_glass_doors(e)
  geoelf.glyphs.assign_glyphs(e, only_trees)
  e.subst(geoelf.glyphs.FOUNTAIN .. " = TTUV")

  if (geoelf.debug) then
    -- print the map to console
    print("MAP")
    local gxm, gym = dgn.max_bounds()
    for y = 0, gym - 1 do
      local line_output = ""
      for x = 0, gxm - 1 do
        line_output = line_output .. e.mapgrd[x][y]
      end
      print(line_output)
    end
  end
end

----------------
--
-- This is the helper function that turns the doors in glass
--  walls into glass doors. Some doors are made as glass
--  doors, but some are not. The ones that are not are fixed
--  here.
--

function geoelf.make_glass_doors (e)
  if (geoelf.debug) then print("  geoelf.make_glass_doors") end

  local gxm, gym = dgn.max_bounds()
  for y = 1, gym - 2 do
    for x = 1, gxm - 2 do
      if (e.mapgrd[x][y] == geoelf.glyphs.DOOR) then
        local south = e.mapgrd[x][y + 1]
        local north = e.mapgrd[x][y - 1]
        local east  = e.mapgrd[x + 1][y]
        local west  = e.mapgrd[x - 1][y]
        local southeast = e.mapgrd[x + 1][y + 1]
        local southwest = e.mapgrd[x - 1][y + 1]
        local northeast = e.mapgrd[x + 1][y - 1]
        local northwest = e.mapgrd[x - 1][y - 1]

        local any_wall = (south     == geoelf.glyphs.WALL or
                          north     == geoelf.glyphs.WALL or
                          east      == geoelf.glyphs.WALL or
                          west      == geoelf.glyphs.WALL or
                          southeast == geoelf.glyphs.WALL or
                          southwest == geoelf.glyphs.WALL or
                          northeast == geoelf.glyphs.WALL or
                          northwest == geoelf.glyphs.WALL)
        local any_glass = (south     == geoelf.glyphs.GLASS or
                           north     == geoelf.glyphs.GLASS or
                           east      == geoelf.glyphs.GLASS or
                           west      == geoelf.glyphs.GLASS or
                           southeast == geoelf.glyphs.GLASS or
                           southwest == geoelf.glyphs.GLASS or
                           northeast == geoelf.glyphs.GLASS or
                           northwest == geoelf.glyphs.GLASS)

        -- we turn the door into a glass door if it has glass
        --  around it but no solid walls
        if (not any_wall and any_glass) then
          e.mapgrd[x][y] = geoelf.glyphs.GLASS_DOOR
        end
      end
    end
  end
end



----------------
--
-- These are helper functions that do not fit well into any
--  other geoelf file.
--
-- They are used to connect the tree of rooms.
--

function geoelf.room_absorb_zone (room_data, zone_old, zone_new)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end

  for i = 0, room_data.count - 1 do
    if (room_data[i].zone == zone_old) then
      room_data[i].zone = zone_new
    end
  end
end

function geoelf.corridor_activate (corridor_data, index)
  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end
  if (index >= corridor_data.count) then
    crawl.mpr("Error: index is past the end of corridor_data")
  end
  if (corridor_data[index].status ~= geoelf.corridors.INACTIVE) then
    crawl.mpr("Error: Corridor " .. index .. " is not inactive = " ..
               geoelf.corridors.INACTIVE .. ", is " ..
                           corridor_data[index].status)
  end

  corridor_data[index].status = geoelf.corridors.ACTIVE
  if (corridor_data[index].block ~= nil) then
    local block_index = corridor_data[index].block
    if (block_index >= corridor_data.count) then
      crawl.mpr("Error: block_index is past the end of corridor_data")
    end
    if (corridor_data[block_index].status ~= geoelf.corridors.INACTIVE) then
      crawl.mpr("Error: Attempting to block non-inactive corridor " ..
                index .. " (status " .. corridor_data[block_index].status ..
                                ")")
    end
    corridor_data[block_index].status = geoelf.corridors.BLOCKED
  end
end

function geoelf.corridor_activate_tree (room_data, corridor_data)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end

  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end

  -- choose a random order to try corridors
  local test_order = {}
  for i = 0, corridor_data.count - 1 do
    local other_index = crawl.random2(i + 1)
    test_order[i] = test_order[other_index]
    test_order[other_index] = i
  end

  -- add required corridors in a random order
  for i = 0, corridor_data.count - 1 do
    local corridor_index = test_order[i]
    if (corridor_data[corridor_index].status == geoelf.corridors.INACTIVE) then
      local room_index1 = corridor_data[corridor_index].room1
      local room_index2 = corridor_data[corridor_index].room2
      local zone1 = room_data[room_index1].zone
      local zone2 = room_data[room_index2].zone

      if(zone1 ~= zone2) then
        -- there is no way between these rooms yet
        geoelf.corridor_activate(corridor_data, corridor_index)
        geoelf.room_absorb_zone(room_data, zone2, zone1)
      end
    end
  end
end

function geoelf.corridor_activate_random (room_data, corridor_data, fraction)
  if (room_data == nil) then
    crawl.mpr("Error: No room data array")
  end
  if (room_data.count == nil) then
    crawl.mpr("Error: Room data array does not have count")
  end

  if (corridor_data == nil) then
    crawl.mpr("Error: No corridor data array")
  end
  if (corridor_data.count == nil) then
    crawl.mpr("Error: Corridor data array does not have count")
  end

  -- choose a random order to try inactive corridors
  local test_order = { count=0 }
  for i = 0, corridor_data.count - 1 do
    if (corridor_data[i].status == geoelf.corridors.INACTIVE) then
      local new_index = test_order.count
      local other_index = crawl.random2(new_index + 1)
      test_order[new_index] = test_order[other_index]
      test_order[other_index] = i
      test_order.count = new_index + 1
    end
  end

  -- add extra corridors in a random order
  for i = 0, (test_order.count * fraction) - 1 do
    local corridor_index = test_order[i]
    if (corridor_data[corridor_index].status == geoelf.corridors.INACTIVE) then
      geoelf.corridor_activate(corridor_data, corridor_index)
    end
  end
end
