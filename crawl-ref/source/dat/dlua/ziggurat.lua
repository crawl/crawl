------------------------------------------------------------------------------
-- ziggurat.lua:
--
-- Code for ziggurats.
--
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
  -- we specify a global eccentricity. If zig_exc=0, then the ellipses
  -- will be circles etc. It is not the actual eccentricity but some
  -- value between 0 and 100. For deformed ellipses and rectangles, make
  -- sure that the map is wider than it is high for the sake of ASCII.

  z.zig_exc = crawl.random2(101)
  z.builder = ziggurat_choose_builder()
  z.colour = ziggurat_wall_colour()
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
  if you.depth() == 1 then
    dgn.persist.ziggurat = { }
    initialise_ziggurat(dgn.persist.ziggurat, portal)
  end
  local builder = zig().builder

  -- Deeper levels can have all monsters awake.
  -- Does never happen at depths 1-4; does always happen at depths 25-27.
  local generate_awake = you.depth() > 4 + crawl.random2(21)
  zig().monster_hook = generate_awake and global_function("ziggurat_awaken_all")

  if builder then
    return ziggurat_builder_map[builder](e)
  end
end

local zigstair = dgn.gridmark

-- the estimated total map area for ziggurat maps of given depth
-- this is (almost) independent of the layout type
local function map_area()
  return 30 + 18 * you.depth() + you.depth() * you.depth()
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

-- Monster sets, in order:
-- Lair, Snake, Swamp, Shoals, Spider, Slime,
-- Orc, Elf, Vaults, Crypt, Tomb,
-- Abyss, Gehenna, Cocytus, Dis, Tartarus,
-- Fire, Ice, Air, Earth, Negative Energy, Holy, Chaos,
-- Giants, Dragons, Draconians, Archers, Conjurers,
-- Pan, Lair Roulette, Vestibule / all Hells
-- By using spec_fn to wrap monster-spec functions, monster weights
-- are adjusted per-set, sometimes scaling by depth and always by zig completion.

mset(with_props(spec_fn(function ()
  local d = math.max(24, math.floor(150 - you.depth() / 4 - you.zigs_completed() * 12))
  local e = 15 + you.zigs_completed() * 2
  local f = 5 + you.zigs_completed() * 9
  local g = 10 + you.zigs_completed() * 12
  return "place:Lair:$ w:" .. d .. " / dire elephant w:" .. e .. " / " ..
         "skyshark w:" .. e .. " /  catoblepas w:" .. e - 5 .. " / " ..
         "spriggan druid w:" .. e - 5 .. " / torpor snail w:" .. f + 5 .. " / " ..
         "hellephant w:" .. g .. " / caustic shrike w:" .. g
end), { weight = 5 }))

mset(spec_fn(function ()
  local d = math.max(6, you.depth() * 2 + (you.zigs_completed() * 3) - 2)
  local e = math.max(1, you.depth() + (you.zigs_completed() * 3) - 18)
  return "place:Snake:$ w:125 / naga sharpshooter w:5 / guardian serpent w:5 / " ..
         "salamander tyrant w:" .. d .. " / nagaraja w:" .. d .. " / " ..
         "quicksilver dragon w:" .. e
end))

mset(spec_fn(function ()
  local d = math.max(10, math.min(120 - you.zigs_completed() * 12, 280 - you.depth() * 10))
  local e = math.floor(10 + you.zigs_completed())
  local f = 5 + you.zigs_completed() * 9
  local g = math.max(1, you.depth() + (you.zigs_completed() * 3) - 9)
  return "place:Swamp:$ w:" .. d .. " / fenstrider witch w:" .. e .. " / " ..
         "tentacled monstrosity w:" .. f .. " / " ..
         "shambling mangrove w:" .. f .. " / green death w:" .. g .. " / " ..
         "golden dragon w:" .. g
end))

mset(with_props(spec_fn(function ()
  local d = math.max(24, math.floor(120 - you.depth() / 4 - you.zigs_completed() * 12))
  local e = 10 + you.zigs_completed() * 12
  return "place:Shoals:$ w:" .. d .. " / merfolk impaler w:5 / " ..
         "merfolk javelineer / merfolk aquamancer / " ..
         "water nymph w:" .. e
end), { weight = 5 }))

mset(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 12
  local e = 30 + you.zigs_completed() * 15
  local f = 5 + you.zigs_completed() * 3
  return "place:Spider:$ w:250 / torpor snail w:" .. d .. " / " ..
         "emperor scorpion w:" .. d .. " / entropy weaver w:" .. d .. " / " ..
         "ghost moth w:" .. e .. " / moth of wrath w:" .. f
end))

mset(spec_fn(function ()
  local d = 5 + 5 * (you.zigs_completed() * 3)
  return "place:Slime:$ w:1500 / glass eye w:" .. d .. " / " ..
         "azure jelly w:" .. d .. " / void ooze w:" .. d .. " / " ..
         "rockslime w:" .. d * 3 .. " / acid blob w:" .. d * 14
end))

mset(spec_fn(function ()
  local d = math.max(2, 380 - 20 * you.depth() - you.zigs_completed() * 6)
  local e = 25 + you.zigs_completed() * 6
  local f = math.max(0, math.floor(you.depth() * 1.5 + you.zigs_completed() * 3 - 35))
  return "place:Orc:$ w:" .. d .. " / orc warlord w:" .. e .. " / " ..
         "orc high priest w:" .. e .. " / orc sorcerer / " ..
         "stone giant / iron troll / juggernaut w:" .. e - 24 .. " / " ..
         "moth of wrath w:" .. f .. " / undying armoury w:" .. f
end))

mset(spec_fn(function ()
  local d = math.max(2, 300 - 10 * you.depth() - you.zigs_completed() * 3)
  local e = math.max(2, math.floor(10 - you.zigs_completed()))
  return "place:Elf:$ w:" .. d .. " / deep elf high priest / " ..
         "deep elf blademaster / deep elf master archer / " ..
         "deep elf annihilator w:" .. e .. " / deep elf demonologist w:" .. e
end))

mset(spec_fn(function ()
  local d = math.max(5, 30 - you.zigs_completed() * 3)
  local e = math.max(1, you.zigs_completed() * 6 + you.depth() / 2 - 11)
  return "place:Vaults:$ w:" .. d * 2 .. " / place:Vaults:$ 9 w:" .. d .. " / " ..
         "sphinx w:5 / titan w:" .. e .. " / golden dragon w:" .. e .. " / " ..
         "ancient lich w:" .. e / 3 .. " / dread lich w:" .. e / 3
end))

mset(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 6
  local e = 10 + you.zigs_completed() * 9
  return "place:Crypt:$ 9 w:260 / " ..
         "curse skull w:" .. d .. " / revenant w:" .. e .. " / " ..
         "ancient lich w:" .. d .. " / dread lich w:" .. e
end))

mset(spec_fn(function ()
  local d = math.max(100, 760 - you.zigs_completed() * 10)
  local e = you.zigs_completed() * 3
  local f = 10 + you.zigs_completed() * 5
  local g = 20 + you.depth() * 4 + you.zigs_completed() * 8
  return "place:Tomb:$ 9 w:" .. d .. " / mummy priest w:" .. e .. " / " ..
         "bennu w:" .. f .. " / royal mummy w:" .. g
end))

mset(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 60
  local e = 10 + you.zigs_completed() * you.zigs_completed() * 2
  return "place:Abyss:$ w:1910 / demonspawn corrupter w:" .. d .. " / " ..
         "starcursed mass w:" .. d .. " / wretched star w:" .. d .. " / " ..
         "bone dragon w:" .. d / 2 .. " / wyrmhole w:" .. d / 2 .. " / " ..
         "daeva w:" .. d .. " / " .. "silent spectre w:" .. e
end))

mset(with_props(spec_fn(function ()
  local d = math.max(10, 455 - you.zigs_completed() * 9)
  local e = 5 + you.zigs_completed() * 6
  local f = 10 + you.zigs_completed() * 15
  return "place:Geh:$ w:" .. d .. " / hellion w:" .. e .. " / " ..
         "oni incarcerator w:" .. e .. " / brimstone fiend w:" .. f
end), { weight = 5 }))

mset(with_props(spec_fn(function ()
  local d = math.max(10, 455 - you.zigs_completed() * 9)
  local e = 5 + you.zigs_completed() * 6
  local f = 10 + you.zigs_completed() * 15
  return "place:Coc:$ w:" .. d .. " / shard shrike w:" .. e .. " / " ..
         "titan w:" .. e .. " / ice fiend w:" .. f
end), { weight = 5 }))

mset(with_props(spec_fn(function ()
  local d = math.max(10, 455 - you.zigs_completed() * 9)
  local e = 5 + you.zigs_completed() * 6
  local f = 10 + you.zigs_completed() * you.zigs_completed() * 10
  return "place:Dis:$ w:" .. d .. " / quicksilver elemental w:" .. e .. " / " ..
         "iron giant w:" .. e .. " / hell sentinel w:" .. f
end), { weight = 5 }))

mset(with_props(spec_fn(function ()
  local d = math.max(20, 1840 - you.zigs_completed() * 36)
  local e = 5 + you.zigs_completed() * 6
  local f = 10 + you.zigs_completed() * you.zigs_completed() * 10
  local g = 0 + you.zigs_completed() * you.zigs_completed() * 2
  return "place:Tar:$ w:" .. d .. " / curse toe w:" .. e .. " / " ..
         "doom hound w:" .. e .. " / tzitzimitl w:" .. f .. " / " ..
         "silent spectre w:" .. g
end), { weight = 5 }))

mset(with_props(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 7
  local e = 10 + you.zigs_completed() * you.zigs_completed() * 10
  return "efreet / fire crab / hell knight / will-o-the-wisp / " ..
         "salamander tyrant w:" .. d .. " / balrug w:" .. d .. " / " ..
         "red draconian scorcher w:" .. d .. " / orb of fire w:" .. e
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 6
  local e = 10 + you.zigs_completed() * you.zigs_completed() * 10
  return "ice devil w:5 / rime drake w:5 / azure jelly / " ..
         "caustic shrike simulacrum w:5 / spriggan defender simulacrum w:5 / " ..
         "juggernaut simulacrum w:5 / ironbound frostheart w:5 / " ..
         "walking frostbound tome w:" .. d .. " / frost giant w:" .. d .. " / " ..
         "blizzard demon w:" .. d .. " / white draconian knight w:" .. e .. " / " ..
         "shard shrike w:" .. e .. " / ice fiend w:" .. e
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 5
  local e = 10 + you.zigs_completed() * 8
  local f = 10 + you.zigs_completed() * you.zigs_completed() * 4
  return "raiju w:5 / wind drake w:5 / air elemental / " ..
         "shock serpent w:" .. d .. " / spark wasp w:" .. d .. " / " ..
         "ironbound thunderhulk w:" .. d .. " / " ..
         "spriggan air mage w:" .. e .. " / storm dragon w:" .. e .. " / " ..
         "electric golem w:" .. e .. " / titan w:" .. f
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 20 + you.zigs_completed() * 5
  local e = 20 + you.zigs_completed() * 8
  local f = 20 + you.zigs_completed() * you.zigs_completed() * 3
  return "gargoyle w:20 / earth elemental w:20 / boulder beetle w:20 / " ..
         "torpor snail w:" .. d .. " / iron golem w:" .. d .. " / " ..
         "war gargoyle w:" .. d .. " / stone giant w:" .. d .. " / " ..
         "caustic shrike w:" .. d .. " / entropy weaver w:" .. d .. " / " ..
         "iron dragon w:" .. d .. " / crystal guardian w:" .. e .. " / " ..
         "undying armoury w:" .. e .. " / iron giant w:" .. f .. " / " ..
         "hell sentinel w:" .. f
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = math.max(2, math.floor((32 - you.depth()) / 5))
  local e = math.min(8, math.floor((you.depth()) / 5) + 4)
  local f = math.max(1, you.depth() + you.zigs_completed() * 2 - 5)
  return "soul eater w:" .. d .. " / phantasmal warrior w:" .. d .. " / " ..
         "deep elf death mage w:2 / shadow dragon w:4 / ghost crab w:4 / " ..
         "eidolon w:" .. e .. " / revenant w:" .. e .. " / " ..
         "demonspawn soul scholar w:4 / curse skull w:4 / curse toe w:2 / " ..
         "player ghost w:" .. f
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 30 + you.zigs_completed() * 6
  local e = 0
  if you.depth() > (13 - you.zigs_completed()) then
    e = math.max(1, math.floor(you.depth() / 3) +
        you.zigs_completed() * you.zigs_completed() * 8 - 16)
  end
  return "ophan w:30 / apis w:30 / cherub w:30 / angel w:30 / fravashi w:30 / " ..
         "daeva w:" .. d .. " / pearl dragon w:" .. d .. " / seraph w:" .. e
end), { weight = 2 }))

mset(spec_fn(function ()
  local d = math.max(2, math.floor((32 - you.depth()) / 5))
  local e = math.min(8, math.floor((you.depth()) / 5) + 4)
  local f = math.max(1, you.depth() + you.zigs_completed() * 2 - 4)
  return "chaos spawn w:" .. d .. " / very ugly thing w:" .. d .. " / " ..
         "apocalypse crab w:4 / killer klown w:8 / " ..
         "shapeshifter hd:16 w:" .. e .. " / " ..
         "glowing shapeshifter w:" .. e / 3 .. " / " ..
         "protean progenitor w:" .. e .. " / " ..
         "pandemonium lord w:" .. f
end))

mset(with_props(spec_fn(function ()
  local d = 20 + you.zigs_completed() * 9
  local e = 20 + you.zigs_completed() * 12
  local f = 20 + you.zigs_completed() * 16
  return "cyclops w:20 / ettin w:20 / " ..
         "stone giant w:" .. d .. " / " .. "fire giant w:" .. d .. " / " ..
         "frost giant w:" .. d .. " / titan w:" .. f .. " / " ..
         "juggernaut w:" .. e .. " / iron giant w:" .. e .. " / " ..
         "protean progenitor w:" .. d
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 20 + you.zigs_completed() * 6
  local e = 20 + you.zigs_completed() * 9
  return "swamp drake / rime drake / wind drake w:20 / death drake w:20 / " ..
         "wyvern / hydra / steam dragon w:20 / acid dragon w:20 / " ..
         "swamp dragon w:" .. d .. " / fire dragon w:" .. d .. " / " ..
         "ice dragon w:" .. d .. " / storm dragon w:" .. d .. " / " ..
         "shadow dragon w:" .. d .. " / iron dragon w:" .. d .. " / " ..
         "quicksilver dragon w:" .. e .. " / golden dragon w:" .. e .. " / " ..
         "wyrmhole w:" .. e
end), { weight = 2 }))

mset(spec_fn(function ()
  local d = 41 - you.depth()
  local e = 40 + you.zigs_completed() * 3
  local f = you.zigs_completed() * you.zigs_completed() * 4
  return "base draconian w:" .. d .. " / nonbase draconian w:" .. e .. " / " ..
         "draconian stormcaller w:" .. f .. " / draconian scorcher w:" .. f
end))

mset(with_props(spec_fn(function ()
  local d = 20 + you.zigs_completed() * 6
  local e = 20 + you.zigs_completed() * 9
  return "centaur w:5 / centaur warrior / yaktaur w:15 / cyclops w:15 / " ..
         "kobold blastminer w:" .. d .. " / faun w:" .. d .. " / " ..
         "yaktaur captain w:" .. d .. " / satyr w:" .. d .. " / " ..
         "stone giant w:" .. e .. " / naga sharpshooter w:" .. e .. " / " ..
         "merfolk javelineer w:" .. e .. " / deep elf master archer w:" .. e .. " / " ..
         "nekomata w:" .. e
end), { weight = 2 }))

mset(with_props(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 6
  local e = 10 + you.zigs_completed() * 12
  return "arcanist w:5 / occultist w:5 / necromancer / ogre mage w:5 / " ..
         "orc sorcerer w:5 / naga mage / salamander mystic w:5 / " ..
         "merfolk aquamancer w:5 / spriggan air mage w:" .. d - 5 .. " / " ..
         "nagaraja w:" .. d .. " / deep elf annihilator w:" .. d .. " / " ..
         "deep elf sorcerer w:" .. d .. " / tengu reaver w:" .. d .. " / " ..
         "draconian knight w:" .. d - 5 .. " / " ..
         "draconian scorcher w:" .. d - 5 .. " / lich w:" .. d - 5 .. " / " ..
         "ancient lich w:" .. d - 5 .. " / " ..
         "demonspawn blood saint w:" .. d .. " / " ..
         "draconian annihilator w:" .. e
end), { weight = 2 }))

local pan_lord_fn = zig_monster_fn("pandemonium lord")
local pan_critter_fn = zig_monster_fn(
         "place:Pan w:" .. math.max(1, 105 - you.zigs_completed() * 12) .. " / " ..
         "greater demon w:" .. math.max(1, 75 - you.zigs_completed() * 7) .. " / " ..
         "brimstone fiend w:5 / ice fiend w:5 / tzitzimitl w:5 / " ..
         "hell sentinel w:5 / nekomata w:2 / demonspawn soul scholar / " ..
         "demonspawn blood saint / demonspawn corrupter / " ..
         "demonspawn warmonger w:" .. 10 + you.zigs_completed())

local function mons_panlord_gen(x, y, nth)
  if nth == 1 then
    dgn.set_random_mon_list("place:Pan w:100 / greater demon w:90 / " ..
         "demonspawn soul scholar / demonspawn blood saint / " ..
         "demonspawn corrupter / demonspawn warmonger")
    return pan_lord_fn(x, y)
  else
    return pan_critter_fn(x, y)
  end
end

mset(mons_panlord_gen)

mset_if(depth_ge(14), with_props(spec_fn(function ()
  local d = math.max(2, math.floor(14 - you.zigs_completed() * 2))
  local e = 10 + you.zigs_completed() * 3
  return "place:Snake:$ w:" .. d .. " / place:Swamp:$ w:" .. d .. " / " ..
         "place:Shoals:$ w:" .. d .. " / place:Spider:$ w:" .. d .. " / " ..
         "nagaraja w:" .. e - 2 .. " / guardian serpent w:5 / " ..
         "fenstrider witch w:5 / tentacled monstrosity w:" .. e - 2 .. " / " ..
         "merfolk aquamancer w:5 / water nymph w:" .. e + 2 .. " / " ..
         "ghost moth w:" .. e + 2 .. " / moth of wrath w:5"
end), { weight = 5 }))

mset_if(depth_ge(14), with_props(spec_fn(function ()
  local d = 10 + you.zigs_completed() * 6
  local e = 66 - (you.depth() * 3)
  local f = math.min(16, you.zigs_completed() * 7 + 5)
  local g = 0 + you.zigs_completed() * you.zigs_completed()
  return "place:Coc:$ w:" .. d .. " / place:Dis:$ w:" .. d .. " / " ..
         "place:Geh:$ w:" .. d .. " / place:Tar:$ w:" .. d .. " / " ..
         "place:Hell w:100 / sin beast w:" .. e .. " / " ..
         "hellion w:5 / tormentor w:5 / greater demon w:" .. f .. " / " ..
         "nargun w:" .. g .. " / quicksilver elemental w:" .. g .. " / " ..
         "searing wretch w:" .. g .. " / silent spectre w:" .. g
end), { weight = 2 }))

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

local function ziggurat_create_monsters(entry, exit, mfn)
  local depth = you.depth()
  local completed = you.zigs_completed()
  local hd_pool = math.floor(10 + (depth * (depth + 8 * (1 + completed * 2) )) * (1 + completed * 0.17))

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
    elseif not dgn.mons_at(point.x, point.y) and entry ~= point then
      mons_do_place(point)
    end
  end

  dgn.find_adjacent_point(exit, mons_place, dgn_passable)
end

local function ziggurat_create_loot_at(c)
  -- Basically, loot grows linearly with depth & zigs finished.
  local depth = you.depth()
  local completed = you.zigs_completed()
  local nloot = depth + crawl.random2(math.floor(depth * 0.5))

  if completed > 0 then
    nloot = math.min(nloot + crawl.random2(math.floor(completed * depth / 4)), map_area() / 9)
  end

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
  local good_loot = dgn.item_spec("* no_pickup w:7000 /" ..
                                  dgn.good_scrolls)
  local super_loot = dgn.item_spec("| no_pickup w:7000 /" ..
                                   "potion of experience no_pickup w:190 q:1 /" ..
                                   "potion of mutation no_pickup w:290 /" ..
                                   "potion of cancellation q:5 no_pickup / " ..
                                   "potion of heal wounds q:5 no_pickup / " ..
                                   "potion of magic q:5 no_pickup / " ..
                                   "potion of haste q:5 no_pickup / " ..
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
  else
    dgn.create_item(exit.x, exit.y, "figurine of a ziggurat")
  end

  zigstair(exit.x, exit.y + 1, "exit_ziggurat")
  zigstair(exit.x, exit.y - 1, "exit_ziggurat")
end

local function ziggurat_furnish(centre, entry, exit)
  ziggurat_stairs(entry, exit)

  local monster_generation = choose_monster_set()

  if type(monster_generation.spec) == "string" then
    dgn.set_random_mon_list(monster_generation.spec)
  end

  ziggurat_place_pillars(centre)

  ziggurat_create_loot_at(exit)

  ziggurat_create_monsters(entry, exit, monster_generation.fn)

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
