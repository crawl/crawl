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

function ks_random_setup(e, norandomexits)
    e.tags("no_pool_fixup")
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

function zot_entry_setup(e)
  e.tags("zot_entry")
  e.place("Depths:$")
  e.orient("float")
  e.kitem("R = midnight gem")
  e.kfeat("O = enter_zot")
  e.mons("patrolling base draconian")
  e.mons("fire dragon / ice dragon / storm dragon / \
          shadow dragon / bone dragon / golden dragon")
  e.mons("patrolling nonbase draconian")
  e.kmons("0 = ettin / rakshasa / glowing shapeshifter w:5 / \
              stone giant w:12 / spriggan berserker w:8 / hell knight w:5")
  e.kmons("9 = fire giant w:12 / titan w:8 / vampire knight / \
              spriggan air mage w:8 / deep troll earth mage w:8 / \
              tengu reaver w:12 / tentacled monstrosity / lich w:2")
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
  if not you.uniques("the Serpent of Hell")
        and you.in_branch(b)
        and you.depth() == dgn.br_depth(b) then
    e.kmons('D = the Serpent of Hell')
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
  local weapon1 = util.random_from(types)
  local weapon2 = weapon1
  local weapon3 = weapon1
  while weapon2 == weapon1 do
    weapon2 = util.random_from(types)
  end
  while weapon3 == weapon1 or weapon3 == weapon2 do
    weapon3 = util.random_from(types)
  end
  local ego1 = util.random_from(egos)
  local ego2 = ego1
  local ego3 = ego1
  while ego2 == ego1 do
    ego2 = util.random_from(egos)
  end
  while ego3 == ego1 or ego3 == ego2 do
    ego3 = util.random_from(egos)
  end
  e.mons("dancing weapon; good_item " .. weapon1 .. " ego:" .. ego1)
  e.mons("dancing weapon; good_item " .. weapon2 .. " ego:" .. ego2)
  e.mons("dancing weapon; good_item " .. weapon3 .. " ego:" .. ego3 .. " / nothing")
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
Set up a KMONS for a master elementalist vault-defined monster. This monster
will have either the elemental staff or a staff of air and a robe of
resistance, so it has all of the elemental resistances.

@tab e The map environment.
@string glyph The glyph on which to define the KMONS.
]]
function master_elementalist_setup(e, glyph, ele_staff)
    local equip_def = " ; elemental staff . robe ego:willpower good_item"
    -- Don't want to use the fallback here, so we can know to give resistance
    -- ego robe if the elemental staff isn't available.
    if you.unrands("elemental staff") then
        equip_def = " ; staff of air . robe ego:resistance good_item"
    end

    e.kmons(glyph .. " = occultist hd:18 name:master_elementalist n_rpl" ..
        " n_des n_noc tile:mons_master_elementalist" ..
        " spells:lehudib's_crystal_spear.11.wizard;" ..
            "chain_lightning.11.wizard;" ..
            "fire_storm.11.wizard;" ..
            "ozocubu's_refrigeration.11.wizard;" ..
            "haste.11.wizard;" ..
            "repel_missiles.11.wizard" .. equip_def)
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
    ["fur brush"] = {"brown", "dngn_yak_fur"},
    ["mop and bucket"] = {"lightblue", "dngn_mop"},
    ["bloodied mop and bucket"] = {"lightred", "dngn_mop_bloody"}
  }

  for name, contents in pairs(dec) do
    if type == name then
      e.colour(glyph .. " = " .. contents[1])
      e.tile(glyph .. " = " .. contents[2])
    end
  end

  e.set_feature_name('decorative_floor', type)
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
    e.mons('ugly thing w:' .. 7 - d .. ' / lindwurm w:4 / ' ..
           'freezing wraith w:4 / crystal guardian w:' .. d)
    e.mons('great orb of eyes w:' .. 7 - d .. ' / ' ..
           'boggart band w:5 / glowing orange brain w:' .. d + 1)
    e.mons('arcanist w:' .. 14 - d * 2 .. ' / sphinx w:10 / ' ..
           'ironbound convoker w:5 / deep elf annihilator w:1')
    e.mons('deep elf annihilator / deep elf sorcerer / lich / tengu reaver')
    e.item('robe / mundane hat')
    e.item('randbook numspells:1 slevels:' .. sl  .. ' / ' ..
           'mundane ring of magical power w:2')
    e.tile('c = wall_studio')
  elseif set == 'icebox' then
    e.tags('no_wall_fixup')
    local f = 'ego:freezing ident:type'
    local c = 'ego:cold_resistance ident:type'
    e.mons('white ugly thing w:' .. 8 - d * 2 .. ' / ' ..
           'redback simulacrum w:' .. 8 - d * 2 .. ' / ' ..
           'freezing wraith w:2 / sphinx simulacrum w:' .. d - 1)
    e.mons('necromancer w:2 / arcanist / white very ugly thing')
    e.mons('ironbound frostheart / frost giant w:1')
    e.mons('golden dragon / tengu reaver w:25 ; halberd ' .. f .. ' | war axe ' .. f ..
                                            ' . ring mail ' .. c)
    e.kitem('d : dagger ' .. f .. ' no_pickup / ' ..
                'long sword ' .. f .. ' no_pickup/ ' ..
                'mace ' .. f .. ' no_pickup')
    e.kitem('e = robe ' .. c .. ' / leather armour ' .. c)
    e.kfeat('m = cache of meat')
    e.kfeat('I = rock_wall')
    e.tile('I = wall_ice_block')
    e.tile('v = dngn_metal_wall_lightblue')
    e.set_feature_name("rock_wall", "ice covered rock wall")
  elseif set == 'garden' then
    e.tags('no_pool_fixup')
    e.mons('harpy w:' .. 80 - d * 20 .. ' / ' ..
           'redback w:' .. 100 - d * 16 .. ' / ' ..
           'wolf spider w:' .. 20 - d * 3 .. ' / ' ..
           'lindwurm w:' .. 140 - d * 20 .. ' / ' ..
           'dire elephant w:' .. d * 3 )
    e.mons('dire elephant w:' .. 18 - d * 3 .. ' / ' ..
           'formless jellyfish w:' .. 2 + d * 4 .. ' / ' ..
           'sphinx w:' .. 2 + d * 2)
    e.mons('ironbound preserver w:' .. 10 - d * 2 .. ' / ' ..
           'entropy weaver w:' .. 2 + d * 3 .. ' / ' ..
           'ironbound beastmaster w:' .. -2 + d * 4)
    e.kmons('S = bush')
    e.kfeat('F = cache of fruit')
    e.ftile('`SF = floor_lair')
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
    e.mons('necromancer w:' .. 10 - d .. ' / ' ..
           'flayed ghost w:' .. 10 - d .. ' / ' ..
           'phantasmal warrior w:' .. d + 2)
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
