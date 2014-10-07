------------------------------------------------------------------------------
-- geoelf_directions.lua:
--
-- Contains the direction constants used by the geoelf layout
--  generator.  Corridors are all in one of these directions,
--  and a room can have at most one corridor leaving it per
--  direction.
------------------------------------------------------------------------------

geoelf.directions = {}    -- Namespace for direction constants, etc.



--
-- The number of directions
--

geoelf.directions.COUNT = 8
geoelf.directions.COUNT_STRAIGHT = 4

--
-- The possible directions
--

geoelf.directions.S     = 0
geoelf.directions.N     = 1
geoelf.directions.E     = 2
geoelf.directions.W     = 3
geoelf.directions.SE    = 4
geoelf.directions.NW    = 5
geoelf.directions.SW    = 6
geoelf.directions.NE    = 7

--
-- There are two possible direction values that can describe
--  each corridor, depending on which way along it you are
--  going.  To avoid redundancy, we declare one of each
--  direction pair to be forwards and one to be backwards.  We
--  will reverse the backwards corridors before drawing them.
--
-- We are using lookup arrays here instead of functions for
--  speed reasons.
--
-- Note that the direction values are being used as the indexes
--  here, not the names.
--

geoelf.directions.IS_REVERSE =
  { [geoelf.directions.S]  = false,
    [geoelf.directions.N]  = true,
    [geoelf.directions.E]  = false,
    [geoelf.directions.W]  = true,
    [geoelf.directions.SE] = false,
    [geoelf.directions.NW] = true,
    [geoelf.directions.SW] = false,
    [geoelf.directions.NE] = true }

geoelf.directions.GET_REVERSE =
  { [geoelf.directions.S]  = geoelf.directions.N,
    [geoelf.directions.N]  = geoelf.directions.S,
    [geoelf.directions.E]  = geoelf.directions.W,
    [geoelf.directions.W]  = geoelf.directions.E,
    [geoelf.directions.SE] = geoelf.directions.NW,
    [geoelf.directions.NW] = geoelf.directions.SE,
    [geoelf.directions.SW] = geoelf.directions.NE,
    [geoelf.directions.NE] = geoelf.directions.SW }

--
-- Moving along any straight directions involve repeatedly
--  either increasing or decreasing the component of the
--  position.  This can be seen as either adding 1 or -1.
--

geoelf.directions.STEP_SIGN =
  { [geoelf.directions.S] = 1,
    [geoelf.directions.N] = -1,
    [geoelf.directions.E] = 1,
    [geoelf.directions.W] = -1 }

--
-- Each diagonal direction is a combination of 2 straight
--  directions.
--

geoelf.directions.COMPONENT_X =
  { [geoelf.directions.SE] = geoelf.directions.E,
    [geoelf.directions.NW] = geoelf.directions.W,
    [geoelf.directions.SW] = geoelf.directions.W,
    [geoelf.directions.NE] = geoelf.directions.E }

geoelf.directions.COMPONENT_Y =
  { [geoelf.directions.SE] = geoelf.directions.S,
    [geoelf.directions.NW] = geoelf.directions.N,
    [geoelf.directions.SW] = geoelf.directions.S,
    [geoelf.directions.NE] = geoelf.directions.N }
