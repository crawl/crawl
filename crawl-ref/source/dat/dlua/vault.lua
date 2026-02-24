--------------------------------------------------------------------------
-- vault.lua: Vault helper functions from more than one file.
--------------------------------------------------------------------------

-- Counting Pan runes for exits.
function count_pan_runes()
  local runes = 0
  for _, r in ipairs({"demonic","glowing","magical","fiery","dark"}) do
    if you.have_rune(r) then
      runes = runes + 1
    end
  end
  return runes
end

-- Functionality for some of HangedMan's decor vaults.
function init_hm_decor_walldepth(e, variables)
local a = you.absdepth() * 5
  if a - 75 > 0 then
    e.subst(variables .. " : x:50 c:" .. a .. " b:" .. a - 75)
  else
    e.subst(variables .. " : x:50 c:" .. a)
  end
end

-- Functionality for Kennysheep vaults.
-- c: outer vault walls
-- x: inner vault walls (could be see-through blocks, or not there at all)
-- t: doodads. fountains, statues or the more decorative walls. Sometimes get replaced by normal walls
-- w: water or lava or trees. or nothing
-- D/F opposite/adjacent vault entrances, that may or may not be there
-- E/H placed in front of the alternate doors. become w tiles if there's no door there

function ks_random_setup(e, extra, norandomexits)
    e.tags("no_pool_fixup")
    if extra then
      e.tags("extra")
      e.depth_weight("Depths", 5)
      e.depth_weight("D", 10)
    end
    -- 1/2 chance the adjacent door is there, followed by a 1/2 chance every
    -- side has a door.
    if norandomexits == nil then
        e.tags("transparent")
        if crawl.one_chance_in(2) then
            e.subst("D : +")
            e.subst("E : .")
            if crawl.one_chance_in(2) then
                e.subst("F : +")
                e.subst("H : .")
            else
                e.subst("F : c")
                e.subst("H : w")
            end
        else
            e.subst("DF : c")
            e.subst("EH : w")
        end
    end
    --rooms have a 1/2 chance of being bordered by water or trees
    --q is used as a placeholder for trees to keep them from being
    -- re-selected in the next step.
    e.subst("w : w.")
    e.subst("w : wwwWWqq")
    --room setups:
    --0 : doodads replaced with walls
    --1 : walls replaced with water/lava or removed. doodads may or may not be walls
    --2 : doodads picked from more obvious vault decorations
    selected = crawl.random2(3)
    if selected == 0 then
        e.subst("t : x")
        e.subst("x : cvbttm")
    elseif selected == 1 then
        e.subst("x : ..wW")
        e.subst("t : .TmbttwG")
    elseif selected == 2 then
        e.subst("t : .TttG")
        e.subst("x : cvbtt")
    end
    -- Turn q into t for tree borders.
    e.subst("q : t")
    -- The outer walls are probably stone or rock, but can be metal or crystal.
    e.subst("c : ccccxxxxvb")
    -- One chance in three of turning all water on the floor into lava.
    -- Tree and fountain tiles are changed so the vault looks normal.
    if crawl.one_chance_in(3) then
        e.subst("Wwt : l")
        e.subst("T : V")
    end
end

function zot_entry_setup(e, use_default_mons)
  e.tags("zot_entry")
  e.place("Depths:$")
  e.orient("float")
  e.kitem("R = midnight gem")
  e.kfeat("O = enter_zot")
  e.kfeat("Z = zot_statue")
  e.kmask("R = no_item_gen")
  if use_default_mons then
    e.mons("patrolling base draconian")
    e.mons("fire dragon w:12 / ice dragon w:12 / storm dragon / \
            shadow dragon / golden dragon w:12 / wyrmhole w:4")
    e.mons("patrolling nonbase draconian")
    e.kmons("0 = ettin / rakshasa / glowing shapeshifter w:5 / \
                stone giant w:12 / spriggan berserker w:8 / hell knight w:5")
    e.kmons("9 = fire giant w:12 / titan w:8 / vampire knight / \
                 spriggan air mage w:8 / deep troll earth mage w:8 / \
                  tengu reaver w:12 / tentacled monstrosity / lich w:2")
  end
end

function soh_hangout()
  if dgn.persist.soh_hangout == nil then
    local hell_branches = { "Geh", "Coc", "Dis", "Tar" }
    dgn.persist.soh_hangout = util.random_from(hell_branches)
  end
  return dgn.persist.soh_hangout
end

-- the Serpent should appear in exactly one hell end. Vaults that call this
-- should guarantee that `D` is present.
function serpent_of_hell_setup(e)
  local b = soh_hangout()
  if not you.uniques("Serpent of Hell")
        and you.in_branch(b)
        and you.depth() == dgn.br_depth(b) then
    e.kmons('D = Serpent of Hell')
  end
end

-- Guarantee two or three rare base types with a brand
function hall_of_blades_weapon(e)
  local long_blade_type = crawl.one_chance_in(2) and "double sword"
                                                  or "triple sword"
  local polearm_type = crawl.one_chance_in(2) and "partisan"
                                               or "bardiche"
  local types = {"eveningstar", "executioner's axe", polearm_type,
                 "lajatang",  "quick blade", long_blade_type}
  local egos = {"flaming", "freezing", "electrocution", "heavy",
                "holy_wrath", "pain", "vampirism",
                "antimagic", "distortion", "spectral"}
  local weapon_t = util.random_subset(types, 3)
  local weapon_e = util.random_subset(egos, 3)
  e.mons("dancing weapon; good_item " .. weapon_t[1] .. " ego:" .. weapon_e[1])
  e.mons("dancing weapon; good_item " .. weapon_t[2] .. " ego:" .. weapon_e[2])
  e.mons("dancing weapon; good_item " .. weapon_t[3] .. " ego:" ..
          weapon_e[3] .. " / nothing")
end

-- Setup for door vaults to define a common loot set and create the door
-- markers.
function door_vault_setup(e)
  -- Don't generate loot in Hells, generate down hatches instead.
  if you.in_branch("Geh") or you.in_branch("Tar") or you.in_branch("Coc")
     or you.in_branch("Dis") then
    e.kfeat("23 = >")
  else
    e.kitem("1 = * / nothing")
    e.kitem("23 = | / nothing")
  end

  -- The marker affects find_connected_range() so that each door opens and
  -- closes separately rather than all of them joining together into a huge
  -- gate that opens all at once.
  e.lua_marker('+',  props_marker { connected_exclude="true" })
  e.lua_marker('a',
               props_marker { stop_explore="strange structure made of doors" })
  e.kfeat("a = runed_clear_door")
end

--[[
Set up a string for a master elementalist vault-defined monster. This monster
will have either the elemental staff or a staff of air and a robe of
resistance, so it has all of the elemental resistances.

@tab e The map environment.
@string glyph The glyph on which to define the KMONS.
]]
function master_elementalist_setup(e, sprintscale)
    local equip_def = " ; elemental staff . robe ego:willpower good_item"
    -- Don't want to use the fallback here, so we can know to give resistance
    -- ego robe if the elemental staff isn't available.
    if you.unrands("elemental staff") then
        equip_def = " ; staff of air . robe randart artprops:rF&&rC&&Will"
    end

    pow = "hd:18"
    name = ""

    -- For use in arenasprint.
    if sprintscale then
        pow = pow .. " hp:200 exp:3950"
        name = "name:grandmaster_elementalist n_rpl n_des n_noc"
    else
        pow = pow .. " hp:100 exp:1425"
        name = "name:master_elementalist n_rpl n_des n_noc"
    end

    return "occultist " .. pow .. " " .. name .. " " ..
           "tile:mons_master_elementalist " ..
           "spells:lehudib's_crystal_spear.11.wizard;" ..
           "chain_lightning.11.wizard;" ..
           "fire_storm.11.wizard;" ..
           "ozocubu's_refrigeration.11.wizard;" ..
           "haste.11.wizard;" ..
           "deflect_missiles.11.wizard" .. equip_def .. " . ring of willpower"
end

-- A function to reduce all the scythe definition boilerplate.
function scythe(ego)
  local s = "halberd itemname:scythe tile:wpn_scythe wtile:scythe"
  if ego ~= nil then
    s = s .. " ego:" .. ego
  end
  return s
end

-- A handy boilerplate-reducing function for getting a cloud generator to place
-- just a single cloud in-place. @fade makes it rarely briefly fade away.
function single_cloud(e, glyph, cloud, fade)
  local pow = ""
  if fade == nil then fade = false end
  if fade then
    pow = "pow_min = 5, pow_max = 7, delay_min = 55, delay_max = 75, excl_rad = 0"
  else
    pow = "pow_min = 90, pow_max = 100, delay = 10, excl_rad = -1"
  end
  e.marker(glyph .. ' = lua:fog_machine { cloud_type = "' .. cloud .. '", ' ..
          "size = 1, " .. pow .. ", walk_dist = 0, " ..
          "start_clouds = 1, spread_rate = 0 }")
end

-- A function to crunch down decorative skeletons.
-- Vaults that only want to place regular branch skeletons or given vault
-- theme skeletons don't need to call this whole function.
function vault_species_skeletons(e, category)
-- (Djinni, gargoyles, mummies, octopodes, poltergeists, revenants don't leave
-- corpses. I don't want to think about how one recognizes vine stalkers
-- post-rotting. Orcs are included to cover Beogh's popularity in the Dungeon.
-- Coglins have their exoskeletons stolen. Species that only show up
-- in extended are counted as the rarest type.
  local s1 = {"goblin", "gnoll", "elf", "kobold", "troll", "orc", "centaur"}
  local s2 = {"draconian", "naga", "merfolk", "minotaur", "spriggan", "tengu"}
  local s3 = {"barachi", "demigod", "dwarf", "demonspawn", "felid", "oni"}
  local output = "human skeleton"
  if category == "early" or category == "dungeon" or category == "all" then
    output = output .. " / " .. table.concat(s1, " skeleton / ")
  end
  if category == "late" or category == "dungeon" or category == "all" then
    output = output .. " / " .. table.concat(s2, " skeleton / ")
  end
  if category == "all" then
    output = output .. " / " .. table.concat(s3, " skeleton / ")
  end
  return output  .. " skeleton"
end

-- Three sets of reusable vault feature redefines scattered across the game,
-- kept in this one function for both consistency and ease of use.
function vault_granite_statue_setup(e, glyph, type)
  e.kfeat(glyph .. " = granite_statue")

  -- Feature name, console colour, tile name.
  local statues = {
    ["broken pillar"] = {"lightgrey", "dngn_crumbled_column"},
    ["gravestone"] = {"lightgrey", "dngn_gravestone"},
    ["sarcophagus"] = {"yellow", "dngn_sarcophagus_pedestal_right"},
    ["scintillating statue"] = {"mountain", "dngn_scintillating_statue"},
  }

  for name, contents in pairs(statues) do
    if type == name then
      e.colour(glyph .. " = " .. contents[1])
      e.tile(glyph .. " = " .. contents[2])
    end
  end

  e.set_feature_name("granite_statue", type)
end

function vault_metal_statue_setup(e, glyph, type)
  e.kfeat(glyph .. " = metal_statue")

  local statues = {
    ["golden statue"] = {"yellow", "dngn_golden_statue"},
    ["silver statue"] = {"silver", "dngn_silver_statue"},
    ["iron statue"] = {"cyan", "dngn_statue_iron"},
    ["mystic cage"] = {"mist", "dngn_mystic_cage"},
    ["arcane conduit"] = {"vehumet", "arcane_conduit"},
    ["misfortune conduit"] = {"shimmer_blue", "misfortune_conduit"},
    ["dimensional conduit"] = {"warp", "dimensional_conduit"},
    ["soul conduit"] = {"smoke", "soul_conduit"},
    ["alchemical conduit"] = {"poison", "alchemical_conduit"},
    ["fiery conduit"] = {"fire", "fiery_conduit"},
    ["icy conduit"] = {"ice", "icy_conduit"},
    ["earthen conduit"] = {"earth", "earthen_conduit"},
    ["storm conduit"] = {"electricity", "storm_conduit"},
    ["enigmatic dynamo"] = {"magic", "dngn_enigmatic_dynamo"},
    ["nascence circuit"] = {"magic", "dngn_nascence_circuit"}
  }

  for name, contents in pairs(statues) do
    if type == name then
      e.colour(glyph .. " = " .. contents[1])
      e.tile(glyph .. " = " .. contents[2])
    end
  end

  e.set_feature_name("metal_statue", type)
end

function decorative_floor (e, glyph, type)
  e.kfeat(glyph .. ' = decorative_floor')

  local dec = {
    ["flower patch"] = {"green", "dngn_flower_patch"},
    ["garden patch"] = {"yellow", "dngn_garden_patch"},
    ["floral vase"] = {"yellow", "dngn_flower_pot"},
    ["mourning vase"] = {"magenta", "dngn_dark_flower_pot"},
    ["broken floral vase"] = {"magenta", "dngn_flower_pot_broken / " ..
                              "dngn_dark_flower_pot_broken"},
    ["orcish standard"] = {"lightcyan", "dngn_ensign_beogh"},
    ["infernal standard"] = {"red", "dngn_ensign_gehenna"},
    ["caliginous standard"] = {"magenta", "dngn_ensign_dark"},
    ["fur brush"] = {"brown", "dngn_yak_fur"},
    ["set of bottled spirits"] = {"lightgreen", "dngn_bottled_spirits"},
    ["djembe set"] = {"brown", "dngn_djembe"},
    ["skull pike"] = {"lightgrey", "dngn_skull_pike"},
    ["mop and bucket"] = {"lightblue", "dngn_mop"},
    ["bloodied mop and bucket"] = {"lightred", "dngn_mop_bloody"},
    ["weapon-inlaid floor"] = {"lightgrey", "floor_blade"}
  }

  for name, contents in pairs(dec) do
    if type == name then
      e.colour(glyph .. " = " .. contents[1])
      e.tile(glyph .. " = " .. contents[2])
    end
  end

  e.set_feature_name('decorative_floor', type)
end

-- The animation implementation for these tiles needs to be each discrete
-- tiles, alas, so to help shorten its use, just use this each time.
function crackle_def(e)
  return "wall_stone_crackle_1 / wall_stone_crackle_2 / " ..
         "wall_stone_crackle_3 / wall_stone_crackle_4"
end

-- A reusable poison / fire / snake theming for statues that show up in Snake.
function snake_statue_setup (e, glyph)
  if crawl.x_chance_in_y(2, 3) then
    vault_metal_statue_setup(e, glyph, "alchemical conduit")
  else
    vault_metal_statue_setup(e, glyph, "fiery conduit")
  end
  e.tile("G : dngn_statue_naga / dngn_statue_archer w:7")
end

-- A function for standardizing some tags and some floor tiles for vaults_hard
-- subvaults.
function vaults_hard_standard(e, ngen, ft)
  e.tags('vaults_hard')
  if ngen == nil then
    ngen = false
  end
  if ngen then
    e.tags('no_monster_gen')
    e.tags('no_item_gen')
  end
  if you.in_branch("Vaults") then
    if not ft then
      e.ftile('0123456789._^~$%*|defghijkmnFGITUVY+mn{([<})]}> = floor_metal_gold')
    else
      e.ftile(ft .. ' = floor_metal_gold')
    end
  end
end

-- A function that uses what's in the mon-pick-data (and bands) for V in 0.32
-- for several distinct, tiered, depths-scaling themes + decorations.
function index_vaults_room_themes (e, set, hard)
  e.tags('luniq_vaults_room_' .. set)

  local d = you.depth() + 1
  if hard then
    e.tags("vaults_hard")
    d = d + 1
  else
    e.tags("vaults_room")
  end

  if set == 'magic' then
    local sl = 1
    if crawl.x_chance_in_y(d, 10) then
      sl = sl + 1
    end
    e.mons('lindwurm w:' .. 7 - d .. ' / crawling flesh cage w:5 / ' ..
           'crystal guardian w:' .. d)
    e.mons('great orb of eyes w:' .. 7 - d .. ' / ' ..
           'boggart band w:5 / glowing orange brain w:' .. d + 1)
    e.mons('arcanist w:' .. 14 - d * 2 .. ' / sphinx marauder w:8 / ' ..
           'ironbound convoker w:4 / ironbound mechanist w:4')
    e.mons('deep elf annihilator / deep elf sorcerer / lich / ' ..
           'guardian sphinx w:5 / tengu reaver')
    e.item('robe / mundane hat')
    e.item('parchment slevel:' .. sl  .. ' / ' ..
           'mundane ring of magical power w:2')
    e.tile('c = wall_studio')
  elseif set == 'icebox' then
    e.tags('no_wall_fixup')
    local f = 'ego:freezing pre_id'
    local c = 'ego:cold_resistance pre_id'
    e.mons('white ugly thing w:' .. 8 - d * 2 .. ' / ' ..
           'harpy simulacrum w:' .. 8 - d * 2 .. ' / ' ..
           'freezing wraith w:2 / guardian sphinx simulacrum w:' .. d - 1)
    e.mons('arcanist / crawling flesh cage')
    e.mons('ironbound frostheart / frost giant w:1')
    e.mons('golden dragon / tengu reaver w:25 ; halberd ' .. f .. ' | war axe ' .. f ..
                                            ' . ring mail ' .. c)
    e.kitem('d : dagger ' .. f .. ' no_pickup / ' ..
                'long sword ' .. f .. ' no_pickup/ ' ..
                'mace ' .. f .. ' no_pickup')
    e.kitem('e = robe ' .. c .. ' / leather armour ' .. c)
    e.kfeat('m = cache_of_meat')
    e.kfeat('I = rock_wall')
    e.tile('I = wall_ice_block')
    e.tile('v = dngn_metal_wall_lightblue')
    e.set_feature_name("rock_wall", "ice covered rock wall")
  elseif set == 'garden' then
    e.tags('no_pool_fixup')
    e.mons('harpy w:' .. 80 - d * 20 .. ' / ' ..
           'wolf spider w:' .. 20 - d * 3 .. ' / ' ..
           'lindwurm w:' .. 140 - d * 20 .. ' / ' ..
           'dire elephant w:' .. d * 3 )
    e.mons('dire elephant w:' .. 18 - d * 3 .. ' / ' ..
           'formless jellyfish w:' .. 2 + d * 4 .. ' / ' ..
           'guardian sphinx w:' .. 2 + d * 2)
    e.mons('ironbound preserver w:' .. 10 - d * 2 .. ' / ' ..
           'entropy weaver w:' .. 2 + d * 3 .. ' / ' ..
           'ironbound beastmaster w:' .. -2 + d * 4)
    e.kmons('S = bush')
    e.kfeat('F = cache_of_fruit')
    e.ftile('`SF = floor_sprouting_stone')
    e.tile('T = dngn_fountain_novelty_fancy')
    e.kitem('d = animal skin / club / whip w:5 / quarterstaff w:5')
    decorative_floor(e, 'p', "garden patch")
  elseif set == 'rangers' then
    local eq = "arbalest w:29 | hand cannon w:1 . " ..
               " scimitar . ring mail | scale mail"
    e.mons('centaur warrior w:' .. 6 - d * 2 .. ' / ' ..
           'yaktaur w:' .. 6 - d * 2 .. ' / ' ..
           'polterguardian w:' .. 6 - d * 2 )
    e.mons('vault sentinel w:' .. 8 - d * 2 .. ' ; '  .. eq .. ' / ' ..
           'orc knight w:' .. 2 + d * 2 .. ' ; '  .. eq .. ' / ' ..
           'yaktaur captain w:' .. 2 + d * 2)
    e.mons('orc warlord w:' .. 2 + d * 2 .. ' ; '  .. eq .. ' / ' ..
           'deep elf high priest w:' .. 2 + d * 3 .. ' / ' ..
           'vault warden w:' .. 2 + d * 4 .. ' ; ' .. eq)
    e.mons('stone giant')
    e.kmons('U = training dummy ; nothing')
    e.item('sling no_pickup w:5 / shortbow no_pickup / arbalest no_pickup')
  elseif set == 'warriorcrypt' then
    e.tags('no_wall_fixup')
    e.mons('vault guard w:' .. 14 - d .. ' / ' ..
           'vault sentinel w:' .. 10 - d .. ' / ' ..
           'orc knight')
    e.mons('phantasmal warrior w:' .. 10 - d .. ' / ' ..
           'flayed ghost w:' .. 10 - d .. ' / ' ..
           'death knight w:' .. d + 2)
    e.mons('war gargoyle w:' .. 16 - d * 2 .. ' / ' ..
           'deep elf death mage w:4 / ' ..
           'undying armoury w:' .. -2 + d)
    e.mons('deep elf death mage w:5 / lich / ancient lich')
    e.item('human skeleton / ogre skeleton w:1 / orc skeleton w:1')
    e.kfeat('u = rock_wall')
    e.tile('u = wall_undead')
    e.tile('c = wall_crypt')
    e.tile('G = dngn_statue_angel / dngn_statue_wraith / dngn_statue_twins / ' ..
               'dngn_statue_dwarf / dngn_statue_sword / dngn_statue_centaur')
  end
end
