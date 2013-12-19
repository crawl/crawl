------------------------------------------------------------------------------
-- ziggurat.lua:
--
-- Code for ziggurats.
--
-- Important notes:
-- ----------------
-- Functions that are attached to Lua markers' onclimb properties
-- cannot be closures, because Lua markers must be saved and closure
-- upvalues cannot (yet) be saved.
------------------------------------------------------------------------------

function zig()
  if not dgn.persist.ziggurat then
    dgn.persist.ziggurat = { }
    -- Initialise here to handle ziggurats accessed directly by &P.
    initialise_ziggurat(dgn.persist.ziggurat)
  end
  return dgn.persist.ziggurat
end

local wall_colours = {
  "blue", "red", "lightblue", "magenta", "green", "white"
}

function ziggurat_wall_colour()
  return util.random_from(wall_colours)
end

function initialise_ziggurat(z, portal)
  -- Any given ziggurat will use the same builder for all its levels,
  -- and the same colours for outer walls. Before choosing the builder,
  -- we specify a global excentricity. If zig_exc=0, then the ellipses
  -- will be circles etc. It is not the actual excentricity but some
  -- value between 0 and 100. For deformed ellipses and rectangles, make
  -- sure that the map is wider than it is high for the sake of ASCII.

  z.zig_exc = crawl.random2(101)
  z.builder = ziggurat_choose_builder()
  z.colour = ziggurat_wall_colour()
end

function callback.ziggurat_initialiser(portal)
  dgn.persist.ziggurat = { }
  initialise_ziggurat(dgn.persist.ziggurat, portal)
end

-- Common setup for ziggurat levels.
function ziggurat_level(e)
  e.tags("allow_dup")
  e.tags("no_dump")
  e.orient("encompass")

  if crawl.game_started() then
    ziggurat_build_level(e)
  end
end

-----------------------------------------------------------------------------
-- Ziggurat level builders.

beh_wander = mons.behaviour("wander")

function ziggurat_awaken_all(mons)
  mons.beh = beh_wander
end

function ziggurat_build_level(e)
  local builder = zig().builder

  -- Deeper levels can have all monsters awake.
  -- Does never happen at depths 1-4; does always happen at depths 25-27.
  local generate_awake = you.depth() > 4 + crawl.random2(21)
  zig().monster_hook = generate_awake and global_function("ziggurat_awaken_all")

  -- Deeper levels may block controlled teleports.
  -- Does never happen at depths 1-6; does always happen at depths 25-27.
  if you.depth() > 6 + crawl.random2(19) then
    dgn.change_level_flags("no_tele_control")
  end

  if builder then
    return ziggurat_builder_map[builder](e)
  end
end

local zigstair = dgn.gridmark

-- the estimated total map area for ziggurat maps of given depth
-- this is (almost) independent of the layout type
local function map_area()
  return 30 + 18*you.depth() + you.depth()*you.depth()
end

local function clamp_in(val, low, high)
  if val < low then
    return low
  elseif val > high then
    return high
  else
    return val
  end
end

local function clamp_in_bounds(x, y)
  return clamp_in(x, 2, dgn.GXM - 3), clamp_in(y, 2, dgn.GYM - 3)
end

local function set_wall_tiles()
  local tileset = {
    blue      = "wall_zot_blue",
    red       = "wall_zot_red",
    lightblue = "wall_zot_cyan",
    magenta   = "wall_zot_magenta",
    green     = "wall_zot_green",
    white     = "wall_vault"
  }

  local wall = tileset[zig().colour]
  if (wall == nil) then
    wall = "wall_vault"
  end
  dgn.change_rock_tile(wall)
end

local function with_props(spec, props)
  local spec_table = type(spec) == "table" and spec or { spec = spec }
  return util.cathash(spec_table, props)
end

local function spec_fn(specfn)
  return { specfn = specfn }
end

local function spec_if(fn, spec)
  local spec_table = type(spec) == "table" and spec or { spec = spec }
  return util.cathash(spec_table, { cond = fn })
end

local function depth_ge(lev)
  return function ()
           return you.depth() >= lev
         end
end

local function depth_range(low, high)
  return function ()
           return you.depth() >= low and you.depth() <= high
         end
end

local function depth_lt(lev)
  return function ()
           return you.depth() < lev
         end
end

local function zig_monster_fn(spec)
  local mfn = dgn.monster_fn(spec)
  return function (x, y)
           local mons = mfn(x, y)
           if mons then
             local monster_hook = zig().monster_hook
             if monster_hook then
               monster_hook(mons)
             end
           end
           return mons
         end
end

local function monster_creator_fn(arg)
  local atyp = type(arg)
  if atyp == "string" then
    local mcreator = zig_monster_fn(arg)

    local function mspec(x, y, nth)
      return mcreator(x, y)
    end
    return { fn = mspec, spec = arg }
  elseif atyp == "table" then
    if not arg.cond or arg.cond() then
      local spec = arg.spec or arg.specfn()
      return util.cathash(monster_creator_fn(spec), arg)
    end
  elseif atyp == "function" then
    return { fn = arg }
  end
end

local mons_populations = { }

local function mset(...)
  util.foreach({ ... }, function (spec)
                          table.insert(mons_populations, spec)
                        end)
end

local function mset_if(condition, ...)
  mset(unpack(util.map(util.curry(spec_if, condition), { ... })))
end

mset(with_props("place:Lair:$ w:85 / catoblepas w:6 / dire elephant w:6 / " ..
                "hellephant w:3", { weight = 5 }),
     with_props("place:Shoals:$ w:125 / merfolk aquamancer /  " ..
                "merfolk impaler w:5 / merfolk javelineer", { weight = 5 }),
     "place:Spider:$ w:115 / ghost moth w:15 / red wasp / " ..
                "orb spider",
     "place:Crypt:$ w:250 / curse skull w:5 / profane servitor w:5 / " ..
                "bone dragon / ancient lich / revenant",
     "place:Forest:$ w:180 / satyr / tengu reaver w:5 / " ..
                "spriggan defender w:5",
     "place:Abyss:$",
     with_props("place:Slime:$", { jelly_protect = true }),
     with_props("place:Coc:$ w:460 / Ice Fiend / " ..
                 "blizzard demon w:30", { weight = 5 }),
     with_props("place:Geh:$ w:460 / Brimstone Fiend / " ..
                 "balrug w:30", { weight = 5 }),
     with_props("place:Dis:$ w:460 / Hell Sentinel / " ..
                 "dancing weapon / iron dragon w:20", { weight = 5 }),
     with_props("place:Tar:$ w:460 / Shadow Fiend / " ..
                 "curse toe / shadow demon w:20", { weight = 5 }),
     with_props("daeva / angel / cherub / pearl dragon / shedu band / ophan / " ..
                "apis / w:5 phoenix / w:5 silver star", { weight = 2 }),
     with_props("hill giant / cyclops / stone giant / fire giant / " ..
                "frost giant / ettin / titan", { weight = 2 }),
     with_props("fire elemental / fire drake / hell hound / efreet / " ..
                "fire dragon / fire giant / orb of fire", { weight = 2 }),
     with_props("ice beast / freezing wraith / ice dragon / " ..
                "frost giant / ice devil / ice fiend / simulacrum w:20 / " ..
                "blizzard demon", { weight = 2 }),
     with_props("insubstantial wisp / air elemental / titan / raiju / " ..
                "storm dragon / electric golem / spriggan air mage", { weight = 2 }),
     with_props("swamp drake / fire drake / wind drake w:2 / death drake / " ..
                "wyvern / hydra / steam dragon / mottled dragon / " ..
                "swamp dragon / fire dragon / ice dragon / storm dragon / " ..
                "iron dragon / shadow dragon / quicksilver dragon / " ..
                "golden dragon", { weight = 2 }),
     with_props("centaur / yaktaur / centaur warrior / yaktaur captain / " ..
                "cyclops / stone giant / faun w:1 / satyr w:2 / thorn hunter w:2 / " ..
                "merfolk javelineer / deep elf master archer", { weight = 2 }))

-- spec_fn can be used to wrap a function that returns a monster spec.
-- This is useful if you want to adjust monster weights in the spec
-- wrt to depth in the ziggurat.
mset(spec_fn(function ()
               local d = 290 - 10 * you.depth()
               local e = math.max(0, you.depth() - 20)
               return "place:Orc:$ w:" .. d .. " / orc warlord / " ..
                 "orc high priest band / orc sorcerer w:5 / stone giant / " ..
                 "iron troll w:5 / moth of wrath w:" .. e
             end))

mset(spec_fn(function ()
               local d = 300 - 10 * you.depth()
               return "place:Elf:$ w:" .. d .. " / deep elf high priest / " ..
                 "deep elf blademaster / deep elf master archer / " ..
                 "deep elf annihilator / deep elf demonologist"
             end))

mset(spec_fn(function ()
               local d = math.max(3, you.depth() - 1)
               local e = math.max(1, you.depth() - 20)
               return "place:Snake:$ w:65 / guardian serpent w:5 / " ..
                 "greater naga w:" .. d .. " / quicksilver dragon w:" .. e
             end))

mset(spec_fn(function ()
               local d = math.max(120, 280 - 10 * you.depth())
               local e = math.max(1, you.depth() - 9)
               return "place:Swamp:$ w:" .. d .. " / hydra / " ..
                 "swamp dragon / green death w:6 / death drake w:1 / " ..
                 "golden dragon w:1 / tentacled monstrosity w:" .. e
             end))

mset(spec_fn(function ()
               local d = math.max(1, you.depth() - 11)
               return "place:Vaults:$ 9 w:30 / place:Vaults:$ w:60 / " ..
                 "titan w:" .. d .. " / golden dragon w:" .. d ..
                 " / ancient lich w:" .. d
             end))

mset(spec_fn(function ()
               local d = you.depth() + 5
               return "place:Tomb:$ w:200 / greater mummy w:" .. d
             end))

mset(spec_fn(function ()
               local d = math.max(2, math.floor((32 - you.depth()) / 5))
               local e = math.min(8, math.floor((you.depth()) / 5) + 4)
               local f = math.max(1, you.depth() - 5)
               return "chaos spawn w:" .. d .. " / ugly thing w:" .. d ..
                 " / very ugly thing w:5 / apocalypse crab w:5 / " ..
                 "shapeshifter hd:16 w:" ..e .. " / glowing shapeshifter w:" .. e ..
                 " / killer klown / pandemonium lord w:" .. f
             end))

mset(spec_fn(function ()
               local d = 41 - you.depth()
               return "base draconian w:" .. d .. " / nonbase draconian w:40"
             end))

local pan_lord_fn = zig_monster_fn("pandemonium lord")
local pan_critter_fn = zig_monster_fn("place:Pan / greater demon w:5")

local function mons_panlord_gen(x, y, nth)
  if nth == 1 then
    local d = math.max(1, you.depth() - 11)
    dgn.set_random_mon_list("place:Pan / greater demon w:5")
    return pan_lord_fn(x, y)
  else
    return pan_critter_fn(x, y)
  end
end

mset_if(depth_ge(8), mons_panlord_gen)

function ziggurat_monster_creators()
  return util.map(monster_creator_fn, mons_populations)
end

local function ziggurat_vet_monster(fmap)
  local fn = fmap.fn
  fmap.fn = function (x, y, nth, hdmax)
              if nth >= dgn.MAX_MONSTERS then
                return nil
              end
              for i = 1, 10 do
                local mons = fn(x, y, nth)
                if mons then
                  -- Discard zero-exp monsters, and monsters that explode
                  -- the HD limit.
                  if mons.experience == 0 or mons.hd > hdmax * 1.3 then
                    mons.dismiss()
                  else
                    -- Monster is ok!
                    return mons
                  end
                end
              end
              -- Give up.
              return nil
            end
  return fmap
end

local function choose_monster_set()
  return ziggurat_vet_monster(
           util.random_weighted_from(
             'weight',
             ziggurat_monster_creators()))
end

-- Function to find travel-safe squares, excluding closed doors.
local dgn_passable = dgn.passable_excluding("closed_door")

local function ziggurat_create_monsters(p, mfn)
  local hd_pool = you.depth() * (you.depth() + 8) + 10

  local nth = 1

  local function mons_do_place(p)
    if hd_pool > 0 then
      local mons = mfn(p.x, p.y, nth, hd_pool)

      if mons then
        nth = nth + 1
        hd_pool = hd_pool - mons.hd

        if nth >= dgn.MAX_MONSTERS then
          hd_pool = 0
        end
      else
        -- Can't find any suitable monster for the HD we have left.
        hd_pool = 0
      end
    end
  end

  local function mons_place(point)
    if hd_pool <= 0 then
      return true
    elseif not dgn.mons_at(point.x, point.y) then
      mons_do_place(point)
    end
  end

  dgn.find_adjacent_point(p, mons_place, dgn_passable)
end

local function ziggurat_create_loot_at(c)
  -- Basically, loot grows linearly with depth. However, the entry fee
  -- affects the loot randomly (separatedly on each stage).
  local depth = you.depth()
  local nloot = depth
  local nloot = depth + crawl.random2(math.floor(nloot * 0.5))

  local function find_free_space(nspaces)
    local spaces = { }
    local function add_spaces(p)
      if nspaces <= 0 then
        return true
      else
        table.insert(spaces, p)
        nspaces = nspaces - 1
      end
    end
    dgn.find_adjacent_point(c, add_spaces, dgn_passable)
    return spaces
  end

  -- dgn.good_scrolls is a list of items with total weight 1000
  local good_loot = dgn.item_spec("* no_pickup w:7000 / " .. dgn.good_scrolls)
  local super_loot = dgn.item_spec("| no_pickup w:7000 /" ..
                                   "potion of experience no_pickup w:200 /" ..
                                   "potion of cure mutation no_pickup w:200 /" ..
                                   "potion of porridge no_pickup w:100 /" ..
                                   "wand of heal wounds no_pickup w:10 / " ..
                                   "wand of hasting no_pickup w:10 / " ..
                                   dgn.good_scrolls)

  local loot_spots = find_free_space(nloot * 4)

  if #loot_spots == 0 then
    return
  end

  local curspot = 0
  local function next_loot_spot()
    curspot = curspot + 1
    if curspot > #loot_spots then
      curspot = 1
    end
    return loot_spots[curspot]
  end

  local function place_loot(what)
    local p = next_loot_spot()
    dgn.create_item(p.x, p.y, what)
  end

  for i = 1, nloot do
    if crawl.one_chance_in(depth) then
      for j = 1, 4 do
        place_loot(good_loot)
      end
    else
      place_loot(super_loot)
    end
  end
end

-- Suitable for use in loot vaults.
function ziggurat_loot_spot(e, key)
  e.lua_marker(key, portal_desc { ziggurat_loot = "X" })
  e.kfeat(key .. " = .")
  e.marker("@ = lua:props_marker({ door_restrict=\"veto\" })")
  e.kfeat("@ = +")
end

local has_loot_chamber = false

local function ziggurat_create_loot_vault(entry, exit)
  local inc = (exit - entry):sgn()

  local connect_point = exit - inc * 3
  local map = dgn.map_by_tag("ziggurat_loot_chamber")

  if not map then
    return exit
  end

  local function place_loot_chamber()
    local res = dgn.place_map(map, true, true)
    if res then
      has_loot_chamber = true
    end
    return res
  end

  local function good_loot_bounds(map, px, py, xs, ys)
    local vc = dgn.point(px + math.floor(xs / 2),
                         py + math.floor(ys / 2))


    local function safe_area()
      local p = dgn.point(px, py)
      local sz = dgn.point(xs, ys)
      local floor = dgn.fnum("floor")
      return dgn.rectangle_forall(p, p + sz - 1,
                                  function (c)
                                    return dgn.grid(c.x, c.y) == floor
                                  end)
    end

    local linc = (exit - vc):sgn()
    -- The map's positions should be at the same increment to the exit
    -- as the exit is to the entrance, else reject the place.
    return (inc == linc) and safe_area()
  end

  local function connect_loot_chamber()
    return dgn.with_map_bounds_fn(good_loot_bounds, place_loot_chamber)
  end

  local res = dgn.with_map_anchors(connect_point.x, connect_point.y,
                                   connect_loot_chamber)
  if not res then
    return exit
  else
    -- Find the square to drop the loot.
    local lootx, looty = dgn.find_marker_position_by_prop("ziggurat_loot")

    if lootx and looty then
      return dgn.point(lootx, looty)
    else
      return exit
    end
  end
end

local function ziggurat_locate_loot(entrance, exit, jelly_protect)
  if jelly_protect then
    return ziggurat_create_loot_vault(entrance, exit)
  else
    return exit
  end
end

local function ziggurat_place_pillars(c)
  local range = crawl.random_range
  local floor = dgn.fnum("floor")

  local map, vplace = dgn.resolve_map(dgn.map_by_tag("ziggurat_pillar"))

  if not map then
    return
  end

  local name = dgn.name(map)

  local size = dgn.point(dgn.mapsize(map))

  -- Does the pillar want to be centered?
  local centered = string.find(dgn.tags(map), " centered ")

  local function good_place(p)
    local function good_square(where)
      return dgn.grid(where.x, where.y) == floor
    end
    return dgn.rectangle_forall(p, p + size - 1, good_square)
  end

  local function place_pillar()
    if centered then
      if good_place(c) then
        return dgn.place_map(map, true, true, c.x, c.y)
      end
    else
      for i = 1, 100 do
        local offset = range(-15, -size.x)
        local offsets = {
          dgn.point(offset, offset) - size + 1,
          dgn.point(offset - size.x + 1, -offset),
          dgn.point(-offset, -offset),
          dgn.point(-offset, offset - size.y + 1)
        }

        offsets = util.map(function (o)
                             return o + c
                           end, offsets)

        if util.forall(offsets, good_place) then
          local function replace(at, hflip, vflip)
            dgn.reuse_map(vplace, at.x, at.y, hflip, vflip)
          end

          replace(offsets[1], false, false)
          replace(offsets[2], false, true)
          replace(offsets[3], true, false)
          replace(offsets[4], false, true)
          return true
        end
      end
    end
  end

  for i = 1, 5 do
    if place_pillar() then
      break
    end
  end
end

local function ziggurat_stairs(entry, exit)
  zigstair(entry.x, entry.y, "stone_arch", "stone_stairs_up_i")

  if you.depth() < dgn.br_depth(you.branch()) then
    zigstair(exit.x, exit.y, "stone_stairs_down_i")
  end

  zigstair(exit.x, exit.y + 1, "exit_ziggurat")
  zigstair(exit.x, exit.y - 1, "exit_ziggurat")
end

local function ziggurat_furnish(centre, entry, exit)
  has_loot_chamber = false
  local monster_generation = choose_monster_set()

  if type(monster_generation.spec) == "string" then
    dgn.set_random_mon_list(monster_generation.spec)
  end

  -- Identify where we're going to place loot, but don't actually put
  -- anything down until we've placed pillars.
  local lootspot = ziggurat_locate_loot(entry, exit,
    monster_generation.jelly_protect)

  if not has_loot_chamber then
    -- Place pillars if we did not create a loot chamber.
    ziggurat_place_pillars(centre)
  end

  ziggurat_create_loot_at(lootspot)

  ziggurat_create_monsters(exit, monster_generation.fn)

  local function needs_colour(p)
    return not dgn.in_vault(p.x, p.y)
      and dgn.grid(p.x, p.y) == dgn.fnum("permarock_wall")
  end

  dgn.colour_map(needs_colour, zig().colour)
  set_wall_tiles()
end

-- builds ziggurat maps consisting of two overimposed rectangles
local function ziggurat_rectangle_builder(e)
  local grid = dgn.grid
  dgn.fill_grd_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

  local area = map_area()
  area = math.floor(area*3/4)

  local cx, cy = dgn.GXM / 2, dgn.GYM / 2

  -- exc is the local eccentricity for the two rectangles
  -- exc grows with depth as 0-1, 1, 1-2, 2, 2-3 ...
  local exc = math.floor(you.depth() / 2)
  if ((you.depth()-1) % 2) == 0 and crawl.coinflip() then
    exc = exc + 1
  end

  local a = math.floor(math.sqrt(area+4*exc*exc))
  local b = a - 2*exc
  local a2 = math.floor(a / 2) + (a % 2)
  local b2 = math.floor(b / 2) + (b % 2)
  local x1, y1 = clamp_in_bounds(cx - a2, cy - b2)
  local x2, y2 = clamp_in_bounds(cx + a2, cy + b2)
  dgn.fill_grd_area(x1, y1, x2, y2, "floor")

  local zig_exc = zig().zig_exc
  local nx1 = cx + y1 - cy
  local ny1 = cy + x1 - cx + math.floor(you.depth()/2*(200-zig_exc)/300)
  local nx2 = cx + y2 - cy
  local ny2 = cy + x2 - cx - math.floor(you.depth()/2*(200-zig_exc)/300)
  nx1, ny1 = clamp_in_bounds(nx1, ny1)
  nx2, ny2 = clamp_in_bounds(nx2, ny2)
  dgn.fill_grd_area(nx1, ny1, nx2, ny2, "floor")

  local entry = dgn.point(x1, cy)
  local exit = dgn.point(x2, cy)

  if you.depth() % 2 == 0 then
    entry, exit = exit, entry
  end

  ziggurat_stairs(entry, exit)
  ziggurat_furnish(dgn.point(cx, cy), entry, exit)
end

-- builds elliptic ziggurat maps
-- given the area, half axes a and b are determined by:
-- pi*a*b=area,
-- a=b for zig_exc=0,
-- a=b*3/2 for zig_exc=100
local function ziggurat_ellipse_builder(e)
  local grid = dgn.grid
  dgn.fill_grd_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

  local zig_exc = zig().zig_exc

  local area = map_area()
  local b = math.floor(math.sqrt(200*area/(200+zig_exc) * 100/314))
  local a = math.floor(b * (200+zig_exc) / 200)
  local cx, cy = dgn.GXM / 2, dgn.GYM / 2

  local floor = dgn.fnum("floor")

  for x=0, dgn.GXM-1 do
    for y=0, dgn.GYM-1 do
      if b*b*(cx-x)*(cx-x) + a*a*(cy-y)*(cy-y) <= a*a*b*b then
        grid(x, y, floor)
      end
    end
  end

  local entry = dgn.point(cx-a+2, cy)
  local exit  = dgn.point(cx+a-2, cy)

  if you.depth() % 2 == 0 then
    entry, exit = exit, entry
  end

  ziggurat_stairs(entry, exit)
  ziggurat_furnish(dgn.point(cx, cy), entry, exit)
end


-- builds hexagonal ziggurat maps
local function ziggurat_hexagon_builder(e)
  local grid = dgn.grid
  dgn.fill_grd_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

  local zig_exc = zig().zig_exc

  local c = dgn.point(dgn.GXM, dgn.GYM) / 2
  local area = map_area()

  local a = math.floor(math.sqrt(2 * area / math.sqrt(27))) + 2
  local b = math.floor(a*math.sqrt(3)/4)

  local left = dgn.point(math.floor(c.x - (a + math.sqrt(2 * a)) / 2),
                         c.y)
  local right = dgn.point(2 * c.x - left.x, c.y)

  local floor = dgn.fnum("floor")

  for x = 1, dgn.GXM - 2 do
    for y = 1, dgn.GYM - 2 do
      local dlx = x - left.x
      local drx = x - right.x
      local dly = y - left.y
      local dry = y - right.y

      if dlx >= dly and drx <= dry
        and dlx >= -dly and drx <= -dry
        and y >= c.y - b and y <= c.y + b then
        grid(x, y, floor)
      end
    end
  end

  local entry = left + dgn.point(1,0)
  local exit  = right - dgn.point(1, 0)

  if you.depth() % 2 == 0 then
    entry, exit = exit, entry
  end

  ziggurat_stairs(entry, exit)
  ziggurat_furnish(c, entry, exit)
end

----------------------------------------------------------------------

ziggurat_builder_map = {
  rectangle = ziggurat_rectangle_builder,
  ellipse = ziggurat_ellipse_builder,
  hex = ziggurat_hexagon_builder
}

local ziggurat_builders = util.keys(ziggurat_builder_map)

function ziggurat_choose_builder()
  return util.random_from(ziggurat_builders)
end
