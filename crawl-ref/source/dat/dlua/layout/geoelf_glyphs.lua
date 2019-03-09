------------------------------------------------------------------------------
-- geoelf_glyphs.lua:
--
-- Contains the glyphs used by the geoelf layout generator.
--  Using other glyphs than these can lead to crashes if they
--  are used as table lookup indexes (which happens).
------------------------------------------------------------------------------

geoelf.glyphs    = {}    -- Namespace for glyph constants, etc.



----------------
--
-- The recognized glyphs
--
-- Most of these are just attaching names to the standard
--  vault-making characters, but I feel nervous hard-coding
--  constants like 'x' into a complex block of code.
--
-- TODO: Special tiles for door in glass and glass depicting
--       plant or feature (together or separately).
--

geoelf.glyphs.WALL           = 'x'
geoelf.glyphs.DOOR           = '+'
geoelf.glyphs.FLOOR          = '.'
geoelf.glyphs.CORRIDOR       = '_'
geoelf.glyphs.TREE           = 't'
geoelf.glyphs.BUSH           = 'D'
geoelf.glyphs.PLANT          = 'E'
geoelf.glyphs.FUNGUS         = 'F'
geoelf.glyphs.STATUE         = 'G'
geoelf.glyphs.FOUNTAIN       = 'T'
geoelf.glyphs.GLASS          = 'm'
geoelf.glyphs.GLASS_DOOR     = '~'
geoelf.glyphs.GLASS_TREE     = 'J'
geoelf.glyphs.GLASS_BUSH     = 'K'
geoelf.glyphs.GLASS_PLANT    = 'L'
geoelf.glyphs.GLASS_FUNGUS   = 'M'
geoelf.glyphs.GLASS_STATUE   = 'N'
geoelf.glyphs.GLASS_FOUNTAIN = 'O'



----------------
--
-- Groups of glyphs
--
-- Names for a group of similar glyphs concatinating into a
--  single string.
--

geoelf.glyphs.ALL_DOORS       = geoelf.glyphs.DOOR ..
                                geoelf.glyphs.GLASS_DOOR
geoelf.glyphs.ALL_GLASS_WALLS = geoelf.glyphs.GLASS ..
                                geoelf.glyphs.GLASS_TREE ..
                                geoelf.glyphs.GLASS_BUSH ..
                                geoelf.glyphs.GLASS_PLANT ..
                                geoelf.glyphs.GLASS_FUNGUS ..
                                geoelf.glyphs.GLASS_STATUE ..
                                geoelf.glyphs.GLASS_FOUNTAIN
geoelf.glyphs.ALL_PLANTLIKE   = geoelf.glyphs.TREE ..
                                geoelf.glyphs.BUSH ..
                                geoelf.glyphs.PLANT ..
                                geoelf.glyphs.FUNGUS
geoelf.glyphs.ALL_FEATURES    = geoelf.glyphs.STATUE ..
                                geoelf.glyphs.FOUNTAIN
geoelf.glyphs.ALL_WALLLIKE    = geoelf.glyphs.WALL ..
                                geoelf.glyphs.ALL_GLASS_WALLS
geoelf.glyphs.ALL_FLOORLIKE   = geoelf.glyphs.FLOOR ..
                                geoelf.glyphs.CORRIDOR ..
                                geoelf.glyphs.ALL_PLANTLIKE ..
                                geoelf.glyphs.ALL_FEATURES



----------------
--
-- These are glyph substitution arrays used by the room-drawing
--  functions. Any glyph drawn that is not in the array will
--  turn into a "nil" when a substitution is made. This will
--  generate an error when the LUA code runs that says something
--  like:
--    /crawl-ref/source/dat/des/builder/layout_geoelf.des:132:
--    /crawl-ref/source/dat/dlua/layout/geoelf_rooms.lua:395:
--    bad argument #3 to '?' (string expected, got nil)
--

-- for room floors
geoelf.glyphs.OPEN_SUBSTITUTIONS =
  { [geoelf.glyphs.WALL]           = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.DOOR]           = geoelf.glyphs.CORRIDOR,
    [geoelf.glyphs.FLOOR]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.CORRIDOR]       = geoelf.glyphs.CORRIDOR,
    [geoelf.glyphs.TREE]           = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.BUSH]           = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.PLANT]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.FUNGUS]         = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.STATUE]         = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.FOUNTAIN]       = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_DOOR]     = geoelf.glyphs.CORRIDOR,
    [geoelf.glyphs.GLASS_TREE]     = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_BUSH]     = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_PLANT]    = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_FUNGUS]   = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_STATUE]   = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS_FOUNTAIN] = geoelf.glyphs.FLOOR
  }

-- for the wall around a room
geoelf.glyphs.BORDER_SUBSTITUTIONS =
  { [geoelf.glyphs.WALL]           = geoelf.glyphs.WALL,
    [geoelf.glyphs.DOOR]           = geoelf.glyphs.DOOR,
    [geoelf.glyphs.FLOOR]          = geoelf.glyphs.GLASS,
    [geoelf.glyphs.CORRIDOR]       = geoelf.glyphs.DOOR,
    [geoelf.glyphs.TREE]           = geoelf.glyphs.GLASS_TREE,
    [geoelf.glyphs.BUSH]           = geoelf.glyphs.GLASS_BUSH,
    [geoelf.glyphs.PLANT]          = geoelf.glyphs.GLASS_PLANT,
    [geoelf.glyphs.FUNGUS]         = geoelf.glyphs.GLASS_FUNGUS,
    [geoelf.glyphs.STATUE]         = geoelf.glyphs.GLASS_STATUE,
    [geoelf.glyphs.FOUNTAIN]       = geoelf.glyphs.GLASS_FOUNTAIN,
    [geoelf.glyphs.GLASS]          = geoelf.glyphs.GLASS,
    [geoelf.glyphs.GLASS_DOOR]     = geoelf.glyphs.GLASS_DOOR,
    [geoelf.glyphs.GLASS_TREE]     = geoelf.glyphs.GLASS_TREE,
    [geoelf.glyphs.GLASS_BUSH]     = geoelf.glyphs.GLASS_BUSH,
    [geoelf.glyphs.GLASS_PLANT]    = geoelf.glyphs.GLASS_PLANT,
    [geoelf.glyphs.GLASS_FUNGUS]   = geoelf.glyphs.GLASS_FUNGUS,
    [geoelf.glyphs.GLASS_STATUE]   = geoelf.glyphs.GLASS_STATUE,
    [geoelf.glyphs.GLASS_FOUNTAIN] = geoelf.glyphs.GLASS_FOUNTAIN,
  }

-- for the line one around the room wall
geoelf.glyphs.OUTLINE_SUBSTITUTIONS =
  { [geoelf.glyphs.WALL]           = geoelf.glyphs.WALL,
    [geoelf.glyphs.DOOR]           = geoelf.glyphs.DOOR,
    [geoelf.glyphs.FLOOR]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.CORRIDOR]       = geoelf.glyphs.CORRIDOR,
    [geoelf.glyphs.TREE]           = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.BUSH]           = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.PLANT]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.FUNGUS]         = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.STATUE]         = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.FOUNTAIN]       = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.GLASS]          = geoelf.glyphs.GLASS,
    [geoelf.glyphs.GLASS_DOOR]     = geoelf.glyphs.GLASS_DOOR,
    [geoelf.glyphs.GLASS_TREE]     = geoelf.glyphs.GLASS_TREE,
    [geoelf.glyphs.GLASS_BUSH]     = geoelf.glyphs.GLASS_BUSH,
    [geoelf.glyphs.GLASS_PLANT]    = geoelf.glyphs.GLASS_PLANT,
    [geoelf.glyphs.GLASS_FUNGUS]   = geoelf.glyphs.GLASS_FUNGUS,
    [geoelf.glyphs.GLASS_STATUE]   = geoelf.glyphs.GLASS_STATUE,
    [geoelf.glyphs.GLASS_FOUNTAIN] = geoelf.glyphs.GLASS_FOUNTAIN,
  }

-- choosing a random feature
geoelf.glyphs.FEATURE_OPTIONS =
  { {geoelf.glyphs.STATUE,   2},
    {geoelf.glyphs.FOUNTAIN, 1} }

-- choosing a random plantlike thing
geoelf.glyphs.PLANTLIKE_OPTIONS =
  { {geoelf.glyphs.TREE,   3},
    {geoelf.glyphs.BUSH,   1},
    {geoelf.glyphs.PLANT,  2},
    {geoelf.glyphs.FUNGUS, 4} }

-- making things into glass if possible
geoelf.glyphs.TO_GLASS =
  { [geoelf.glyphs.WALL]           = geoelf.glyphs.GLASS,
    [geoelf.glyphs.DOOR]           = geoelf.glyphs.GLASS_DOOR,
    [geoelf.glyphs.FLOOR]          = geoelf.glyphs.FLOOR,
    [geoelf.glyphs.CORRIDOR]       = geoelf.glyphs.CORRIDOR,
    [geoelf.glyphs.TREE]           = geoelf.glyphs.GLASS_TREE,
    [geoelf.glyphs.BUSH]           = geoelf.glyphs.GLASS_BUSH,
    [geoelf.glyphs.PLANT]          = geoelf.glyphs.GLASS_PLANT,
    [geoelf.glyphs.FUNGUS]         = geoelf.glyphs.GLASS_FUNGUS,
    [geoelf.glyphs.STATUE]         = geoelf.glyphs.GLASS_STATUE,
    [geoelf.glyphs.FOUNTAIN]       = geoelf.glyphs.GLASS_FOUNTAIN,
    [geoelf.glyphs.GLASS]          = geoelf.glyphs.GLASS,
    [geoelf.glyphs.GLASS_DOOR]     = geoelf.glyphs.GLASS_DOOR,
    [geoelf.glyphs.GLASS_TREE]     = geoelf.glyphs.GLASS_TREE,
    [geoelf.glyphs.GLASS_BUSH]     = geoelf.glyphs.GLASS_BUSH,
    [geoelf.glyphs.GLASS_PLANT]    = geoelf.glyphs.GLASS_PLANT,
    [geoelf.glyphs.GLASS_FUNGUS]   = geoelf.glyphs.GLASS_FUNGUS,
    [geoelf.glyphs.GLASS_STATUE]   = geoelf.glyphs.GLASS_STATUE,
    [geoelf.glyphs.GLASS_FOUNTAIN] = geoelf.glyphs.GLASS_FOUNTAIN,
  }



----------------
--
-- assign_glyphs
--
-- A function that assigns the feature types, tiles, and colours
--  to the glyphs with special meanings.
--

function geoelf.glyphs.assign_glyphs (e, only_trees)
  if (geoelf.debug) then print("  geoelf.glyphs.assign_glyphs") end

  -- Bushes, plant, and fungus
  --  -> If only_trees is true, these all become trees
  if (only_trees) then
    e.subst(geoelf.glyphs.BUSH   ..
            geoelf.glyphs.PLANT  ..
            geoelf.glyphs.FUNGUS .. " = " .. geoelf.glyphs.TREE)
    e.subst(geoelf.glyphs.GLASS_BUSH   ..
            geoelf.glyphs.GLASS_PLANT  ..
            geoelf.glyphs.GLASS_FUNGUS .. " = " .. geoelf.glyphs.GLASS_TREE)
  else
    e.kfeat(geoelf.glyphs.BUSH   ..
            geoelf.glyphs.PLANT  ..
            geoelf.glyphs.FUNGUS .. " = floor")
    e.kmons(geoelf.glyphs.BUSH   .. " = bush")
    e.kmons(geoelf.glyphs.PLANT  .. " = plant")
    e.kmons(geoelf.glyphs.FUNGUS .. " = fungus")

    -- prevent bushes, plants, fungus from being overwritten by vaults
    e.kmask(geoelf.glyphs.BUSH   ..
            geoelf.glyphs.PLANT  ..
            geoelf.glyphs.FUNGUS .. " = vault")
  end

  -- Glass doors
  e.kfeat( geoelf.glyphs.GLASS_DOOR .. " = closed_clear_door")

  -- Glass walls with deliberate images
  --  -> Very rare in practice
  --  -> These are only relevant in tiles
  --  -> We need special tiles for these
  --  -> For now, these are just coloured glass
  e.kfeat(geoelf.glyphs.GLASS_TREE     ..
          geoelf.glyphs.GLASS_BUSH     ..
          geoelf.glyphs.GLASS_PLANT    ..
          geoelf.glyphs.GLASS_FUNGUS   ..
          geoelf.glyphs.GLASS_STATUE   ..
          geoelf.glyphs.GLASS_FOUNTAIN .. " = clear_rock_wall")
  e.colour(geoelf.glyphs.GLASS_TREE     .. " = green")
  e.colour(geoelf.glyphs.GLASS_BUSH     .. " = green")
  e.colour(geoelf.glyphs.GLASS_PLANT    .. " = green")
  e.colour(geoelf.glyphs.GLASS_FUNGUS   .. " = brown")
  e.colour(geoelf.glyphs.GLASS_STATUE   .. " = white")
  e.colour(geoelf.glyphs.GLASS_FOUNTAIN .. " = blue")
  e.tile( geoelf.glyphs.GLASS_TREE     .. " = dngn_transparent_wall_green")
  e.tile( geoelf.glyphs.GLASS_BUSH     .. " = dngn_transparent_wall_green")
  e.tile( geoelf.glyphs.GLASS_PLANT    .. " = dngn_transparent_wall_green")
  e.tile( geoelf.glyphs.GLASS_FUNGUS   .. " = dngn_transparent_wall_brown")
  e.tile( geoelf.glyphs.GLASS_STATUE   .. " = dngn_transparent_wall_white")
  e.tile( geoelf.glyphs.GLASS_FOUNTAIN .. " = dngn_transparent_wall_blue")
  --e.tile( geoelf.glyphs.GLASS_TREE     .. " = stained_glass_tree")
  --e.tile( geoelf.glyphs.GLASS_BUSH     .. " = stained_glass_bush")
  --e.tile( geoelf.glyphs.GLASS_PLANT    .. " = stained_glass_plant")
  --e.tile( geoelf.glyphs.GLASS_FUNGUS   .. " = stained_glass_fungus")
  --e.tile( geoelf.glyphs.GLASS_STATUE   .. " = stained_glass_statue")
  --e.tile( geoelf.glyphs.GLASS_FOUNTAIN .. " = stained_glass_fountain")
end
