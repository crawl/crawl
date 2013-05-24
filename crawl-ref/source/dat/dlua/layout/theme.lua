------------------------------------------------------------------------------
-- theme.lua: Theme functions for different branches
--
------------------------------------------------------------------------------

theme = {}
theme.D = {}

-- Tileset for cavelike D layouts. Optionally can use an emerald caverns
-- theme.
function theme.D.caves(e, allow_crystal)
  if not you.in_branch("D") then return; end
  e.lrocktile('wall_pebble_lightgray')
  e.lfloortile('floor_pebble_brown')

  if (allow_crystal and crawl.one_chance_in(5)) then
    e.subst('x = b')
    e.tile('b = wall_emerald')
    e.lfloortile('floor_pebble_green')
  end
end
