--------------------------------------------------------------------------
-- vault.lua: Vault helper functions from more than one file.
--------------------------------------------------------------------------

-- Counting Pan runes for Ignacio and for exits.
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
        if crawl.one_chance_in(2) then
            e.tags("transparent")
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
    e.subst("w : wwwWWqt")
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
