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
  local types = {"quick blade", long_blade_type,
                 "executioner's axe", "eveningstar", "bardiche",
                 "lajatang"}
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

-- A poison & fire theming for statues that show up in Snake.
function snake_statue_setup (e, glyph)
  e.kfeat(glyph .. " = metal_statue")
  if crawl.x_chance_in_y(2, 3) then
    e.colour(glyph .. " = poison")
    e.tile(glyph .. " = alchemical_conduit")
    e.set_feature_name("metal_statue", "alchemical conduit")
  else
    e.colour(glyph .. " = fire")
    e.tile(glyph .. " = fiery_conduit")
    e.set_feature_name("metal_statue", "fiery conduit")
  end
  e.tile("G : dngn_statue_naga / dngn_statue_archer w:7")
end
