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

require("clua/lm_toll.lua")

-- Deepest you can go in a ziggurat - at this point it's beyond
-- obvious that we're not challenging the player, and one could hope
-- she has enough loot by now.
ZIGGURAT_MAX = 27

function zig()
  if not dgn.persist.ziggurat or not dgn.persist.ziggurat.depth then
    dgn.persist.ziggurat = { }
    -- Initialise here to handle ziggurats accessed directly by &P.
    initialise_ziggurat(dgn.persist.ziggurat)
  end
  return dgn.persist.ziggurat
end

function cleanup_ziggurat()
  return one_way_stair {
    onclimb = function()
                dgn.persist.ziggurat = { }
              end,
    dstplace = zig().origin_level
  }
end

local wall_colours = {
  "blue", "red", "lightblue", "magenta", "green", "white"
}

function ziggurat_wall_colour()
  return util.random_from(wall_colours)
end

function initialise_ziggurat(z, portal)
  if portal then
    z.portal = portal.props
  end

  z.depth = 1

  -- Any given ziggurat will use the same builder for all its levels,
  -- and the same colours for outer walls. Before choosing the builder,
  -- we specify a global excentricity. If zig_exc=0, then the ellipses
  -- will be circles etc. It is not the actual excentricity but some
  -- value between 0 and 100. For deformed ellipses and rectangles, make
  -- sure that the map is wider than it is high for the sake of ASCII.

  z.zig_exc = crawl.random2(101)
  z.builder = ziggurat_choose_builder()
  z.colour = ziggurat_wall_colour()
  z.level  = { }

  z.origin_level = dgn.level_name(dgn.level_id())
end

function ziggurat_initialiser(portal)
  -- First ziggurat will be initialised twice.
  initialise_ziggurat(zig(), portal)
end

local function random_floor_colour()
  return ziggurat_wall_colour()
end

-- Increments the depth in the ziggurat when the player takes a
-- downstair in the ziggurat.
function zig_depth_increment()
  zig().depth = zig().depth + 1
  zig().level = { }
end

-- Returns the current depth in the ziggurat.
local function zig_depth()
  return zig().depth or 0
end

-- Common setup for ziggurat entry vaults.
function ziggurat_portal(e)
  local d = crawl.roll_dice
  local entry_fee =
    10 * math.floor(200 + d(3,200) / 3 + d(10) * d(10) * d(10))

  local function stair()
    return toll_stair {
      amount = entry_fee,
      toll_desc = "to enter a ziggurat",
      desc = "gateway to a ziggurat",
      overmap = "Ziggurat",
      overmap_note = "" .. entry_fee .. " gp",
      dst = "ziggurat",
      dstname = "Ziggurat:1",
      dstname_abbrev = "Zig:1",
      dstorigin = "on level 1 of a ziggurat",
      floor = "stone_arch",
      onclimb = ziggurat_initialiser
    }
  end

  e.lua_marker("O", stair)
  e.kfeat("O = enter_portal_vault")
end

-- Common setup for ziggurat levels.
function ziggurat_level(e)
  e.tags("ziggurat")
  e.tags("allow_dup")
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
  if zig().depth == 1 then
    e.welcome("You land on top of a ziggurat so tall you cannot make out the ground.")
  end
  local builder = zig().builder

  local depth = zig().depth

  -- Deeper levels can have all monsters awake.
  -- Does never happen at depths 1-4; does always happen at depths 25-27.
  local generate_awake = depth > 4 + crawl.random2(21)
  zig().monster_hook = generate_awake and ziggurat_awaken_all

  -- Deeper levels may block controlled teleports.
  -- Does never happen at depths 1-6; does always happen at depths 25-27.
  if depth > 6 + crawl.random2(19) then
    dgn.change_level_flags("no_tele_control")
  end

  if builder then
    return ziggurat_builder_map[builder](e)
  end
end

local zigstair = dgn.gridmark

-- Creates a Lua marker table that increments ziggurat depth.
local function zig_go_deeper()
  local newdepth = zig().depth + 1
  return one_way_stair {
    onclimb = zig_depth_increment,
    dstname = "Ziggurat:" .. newdepth,
    dstname_abbrev = "Zig:" .. newdepth,
    dstorigin = "on level " .. newdepth .. " of a ziggurat"
  }
end

-- the estimated total map area for ziggurat maps of given depth
-- this is (almost) independent of the layout type
local function map_area()
  return 30 + 18*zig_depth() + zig_depth()*zig_depth()
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

local function set_tiles_for_place(place)
  local rock = dgn.lev_rocktile(place)
  dgn.change_rock_tile(rock)

  local floor = dgn.lev_floortile(place)
  dgn.change_floor_tile(floor)
end

local function set_floor_colour(colour)
  if not zig().level.floor_colour then
    zig().level.floor_colour = colour
    dgn.change_floor_colour(colour)
  end
end

local function set_random_floor_colour()
  set_floor_colour( random_floor_colour() )
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
           return zig().depth >= lev
         end
end

local function depth_range(low, high)
  return function ()
           local depth = zig().depth
           return depth >= low and depth <= high
         end
end

local function depth_lt(lev)
  return function ()
           return zig().depth < lev
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
    local _, _, branch = string.find(arg, "^place:(%w+):")
    local _, _, place = string.find(arg, "^place:(%w+):?")
    local mcreator = zig_monster_fn(arg)

    local function mspec(x, y, nth)
      if branch then
        set_floor_colour(dgn.br_floorcol(branch))
      end
      if place then
        set_tiles_for_place(place)
      end
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

mset(with_props("place:Slime:$", { jelly_protect = true }),
     "place:Snake:$",
     with_props("place:Lair:$", { weight = 5 }),
     "place:Crypt:$",
     "place:Abyss",
     with_props("place:Shoal:$", { weight = 5 }),
     with_props("place:Coc:$", { weight = 5 }),
     with_props("place:Geh:$", { weight = 5 }),
     with_props("place:Dis:$", { weight = 5 }),
     with_props("place:Tar:$", { weight = 5 }),
     with_props("daeva / angel", { weight = 2 }))

-- spec_fn can be used to wrap a function that returns a monster spec.
-- This is useful if you want to adjust monster weights in the spec
-- wrt to depth in the ziggurat. At level-generation time, the spec
-- returned by this function will also be used to init the monster
-- population (with dgn.set_random_mon_list). As an example:
mset(spec_fn(function ()
               local d = math.max(0, zig().depth - 12)
               return "place:Vault:$ w:60 / ancient lich w:" .. d
             end))

mset(spec_fn(function ()
               local d = math.max(0, zig().depth - 5)
               return "place:Pan w:40 / pandemonium lord w:" .. d
             end))

mset(spec_fn(function ()
               local d = zig().depth + 5
               return "place:Tomb:$ w:200 / greater mummy w:" .. d
             end))

mset(spec_fn(function ()
               local d = 300 - 10 * zig().depth
               return "place:Elf:$ w:" .. d .. " / deep elf sorcerer / " ..
                 "deep elf blademaster / deep elf master archer / " ..
                 "deep elf annihilator / deep elf demonologist"
             end))

mset(spec_fn(function ()
               local d = 310 - 10 * zig().depth
               local e = math.max(0, zig().depth - 20)
               return "place:Orc:$ w:" .. d .. " / orc warlord / orc knight / " ..
                 "orc high priest w:5 / orc sorcerer w:5 / stone giant / " ..
                 "moth of wrath w:" .. e
             end))


local drac_creator = zig_monster_fn("random draconian")
local function mons_drac_gen(x, y, nth)
  if nth == 1 then
    dgn.set_random_mon_list("random draconian")
  end
  set_random_floor_colour()
  return drac_creator(x, y)
end

local pan_lord_fn = zig_monster_fn("pandemonium lord")
local pan_critter_fn = zig_monster_fn("place:Pan")

local function mons_panlord_gen(x, y, nth)
  set_random_floor_colour()
  if nth == 1 then
    dgn.set_random_mon_list("place:Pan")
    return pan_lord_fn(x, y)
  else
    return pan_critter_fn(x, y)
  end
end

mset_if(depth_ge(6), mons_drac_gen)
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
                    if mons.muse == "eats_items" then
                      zig().level.jelly_protect = true
                    end
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
  local depth = zig_depth()
  local hd_pool = depth * (depth + 8)
-- (was depth * (depth + 8) before and too easy)

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
  local depth = zig_depth()
  local nloot = depth
  nloot = nloot + crawl.random2(math.floor(nloot * zig().portal.amount / 10000))

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

  local loot_depth = 20
  if you.absdepth() > loot_depth then
    loot_depth = you.absdepth() - 1
  end

  local good_loot = dgn.item_spec("*")
  local super_loot = dgn.item_spec("|")

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
    dgn.create_item(p.x, p.y, what, loot_depth)
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
  e.marker("@ = feat: permarock_wall")
  e.kfeat("@ = +")
end

local function ziggurat_create_loot_vault(entry, exit)
  local inc = (exit - entry):sgn()

  local function find_door_spot(p)
    while not dgn.is_wall(dgn.grid(p.x, p.y)) do
      p = p + inc
    end
    return p
  end

  local connect_point = exit - inc * 3
  local map = dgn.map_by_tag("ziggurat_loot_chamber")

  if not map then
    return exit
  end

  local function place_loot_chamber()
    local res = dgn.place_map(map, false, true)
    if res then
      zig().level.loot_chamber = true
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
    local lootx, looty = dgn.find_marker_prop("ziggurat_loot")

    if lootx and looty then
      return dgn.point(lootx, looty)
    else
      return exit
    end
  end
end

local function ziggurat_locate_loot(entrance, exit)
  if zig().level.jelly_protect then
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
        return dgn.place_map(map, false, true, c.x, c.y)
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

  if zig().depth < ZIGGURAT_MAX then
    zigstair(exit.x, exit.y, "stone_stairs_down_i", zig_go_deeper)
  end

  zigstair(exit.x, exit.y + 1, "exit_portal_vault", cleanup_ziggurat())
  zigstair(exit.x, exit.y - 1, "exit_portal_vault", cleanup_ziggurat())
end

local function ziggurat_furnish(centre, entry, exit)
  local monster_generation = choose_monster_set()

  if type(monster_generation.spec) == "string" then
    dgn.set_random_mon_list(monster_generation.spec)
  end

  -- If we're going to spawn jellies, do our loot protection thing.
  if monster_generation.jelly_protect then
    zig().level.jelly_protect = true
  end

  -- Identify where we're going to place loot, but don't actually put
  -- anything down until we've placed pillars.
  local lootspot = ziggurat_locate_loot(entry, exit)

  if not zig().level.loot_chamber then
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
end

-- builds ziggurat maps consisting of two overimposed rectangles
local function ziggurat_rectangle_builder(e)
  local grid = dgn.grid
  dgn.fill_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

  local area = map_area()
  area = math.floor(area*3/4)

  local cx, cy = dgn.GXM / 2, dgn.GYM / 2

  -- exc is the local eccentricity for the two rectangles
  -- exc grows with depth as 0-1, 1, 1-2, 2, 2-3 ...
  local exc = math.floor(zig().depth / 2)
  if ((zig().depth-1) % 2) == 0 and crawl.coinflip() then
    exc = exc + 1
  end

  local a = math.floor(math.sqrt(area+4*exc*exc))
  local b = a - 2*exc
  local a2 = math.floor(a / 2) + (a % 2)
  local b2 = math.floor(b / 2) + (b % 2)
  local x1, y1 = clamp_in_bounds(cx - a2, cy - b2)
  local x2, y2 = clamp_in_bounds(cx + a2, cy + b2)
  dgn.fill_area(x1, y1, x2, y2, "floor")

  local zig_exc = zig().zig_exc
  local nx1 = cx + y1 - cy
  local ny1 = cy + x1 - cx + math.floor(zig().depth/2*(200-zig_exc)/300)
  local nx2 = cx + y2 - cy
  local ny2 = cy + x2 - cx - math.floor(zig().depth/2*(200-zig_exc)/300)
  nx1, ny1 = clamp_in_bounds(nx1, ny1)
  nx2, ny2 = clamp_in_bounds(nx2, ny2)
  dgn.fill_area(nx1, ny1, nx2, ny2, "floor")

  local entry = dgn.point(x1, cy)
  local exit = dgn.point(x2, cy)

  if zig_depth() % 2 == 0 then
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
  dgn.fill_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

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

  if zig_depth() % 2 == 0 then
    entry, exit = exit, entry
  end

  ziggurat_stairs(entry, exit)
  ziggurat_furnish(dgn.point(cx, cy), entry, exit)
end


-- builds hexagonal ziggurat maps
local function ziggurat_hexagon_builder(e)
  local grid = dgn.grid
  dgn.fill_area(0, 0, dgn.GXM - 1, dgn.GYM - 1, "permarock_wall")

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

  if zig_depth() % 2 == 0 then
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
