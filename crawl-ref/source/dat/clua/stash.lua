---------------------------------------------------------------------------
-- stash.lua
-- Annotates items for the stash-tracker's search, and for autopickup
-- exception matches.
--
-- Available annotations:
-- {dropped} for dropped items.
-- {throwable} for items you can throw.
-- {artefact} for artefacts.
-- {ego} for identified branded items.
-- { <skill> } - the relevant weapon skill for weapons.
-- { <class> } - item class: gold, weapon, missile, wand, carrion, food,
--               scroll, jewellery, potion, book, magical staff, orb, misc,
--               <armourtype> armour
-- { <ego> } - short item ego description: rC+, rPois, SInv, freeze etc.
-- {god gift} for god gifts
-- {fruit} for fruit
--
-- Item annotations are always prefixed to the item name. For instance:
-- {artefact} the Staff of Wucad Mu
---------------------------------------------------------------------------

-- Annotate items for searches
function ch_stash_search_annotate_item(it)
  local annot = ""

  if it.dropped then
    annot = annot .. "{dropped} "
  end

  if it.ininventory then
    annot = annot .. "{inventory} "
  end

  if it.is_in_shop then
    annot = annot .. "{in_shop} "
  end

  if it.is_throwable then
    annot = annot .. "{throwable} "
  end

  if it.artefact then
    annot = annot .. "{artefact} {artifact} "
  elseif it.branded then
    annot = annot .. "{ego} {branded} "
  end

  if it.god_gift then
    annot = annot .. "{god gift} "
  end

  if food.isfruit(it) then
    annot = annot .. "{fruit} "
  end

  local skill = it.weap_skill
  if skill then
    annot = annot .. "{" .. skill .. "} "
  end

  if it.ego_type_terse ~= "" and it.ego_type_terse ~= "unknown" then
    if it.class(true) == "jewellery" then
      annot = annot .. "{" .. it.ego_type_terse
      if it.ego_type_terse == "Ice" then
        annot = annot .. " rC+ rF-"
      elseif it.ego_type_terse == "Fire" then
        annot = annot .. " rF+ rC-"
      elseif it.ego_type_terse == "Str" or it.ego_type_terse == "Int"
         or it.ego_type_terse == "Dex" or it.ego_type_terse == "Slay"
         or it.ego_type_terse == "EV" or it.ego_type_terse == "AC" then
        if it.plus == nil then
          annot = annot .. "+"
        else
          annot = annot .. string.format("%+d", it.plus)
        end
      end
      annot = annot .. "} "
    else
      annot = annot .. "{" .. it.ego_type_terse .. "} "
    end
  end

  if it.class(true) == "magical staff" and not it.artefact then
    local props = {
      ["air"] = "rElec",
      ["cold"] = "rC+",
      ["death"] = "rN+",
      ["energy"] = "channel",
      ["fire"] = "rF+",
      ["poison"] = "rPois",
      ["power"] = "MP+",
      ["wizardry"] = "Wiz"
    }
    if props[it.subtype()] then
      annot = annot .. "{" .. props[it.subtype()] .. "} "
    end
  end

  if it.class(true) == "armour" and not it.artefact then
    local props = {
      ["troll"] = "Regen+",
      ["steam"] = "rSteam",
      ["acid"] = "rCorr",
      ["quicksilver"] = "MR+",
      ["swamp"] = "rPois",
      ["fire"] = "rF++ rC-",
      ["ice"] = "rC++ rF-",
      ["pearl"] = "rN+",
      ["storm"] = "rElec",
      ["shadow"] = "Stlth++++",
      ["gold"] = "rF+ rC+ rPois"
    }
    local t = it.name("base"):match("%w+")
    if props[t] then
      annot = annot .. "{" .. props[t] .. "} "
    end
  end

  if it.class(true) == "potion" and it.is_preferred_food then
    annot = annot .. "{food} "
  end

  if it.class(true) == "armour" then
      annot = annot .. "{" .. it.subtype() .. " "
  elseif it.class(true) == "weapon" then
      if it.is_ranged then
        annot = annot .. "{ranged "
      else
        annot = annot .. "{melee "
      end
  else
      annot = annot .. "{"
  end
  annot = annot .. it.class(true) .. "}"

  if it.class(true) == "armour" then
      annot = annot .. " {" .. it.subtype() .. " armor}"
  end

  local resistances = {
    ["MR+"] = "magic",
    ["rC+"] = "cold",
    ["rCorr"] = "corrosion",
    ["rElec"] = "electricity",
    ["rF+"] = "fire",
    ["rMut"] = "mutation",
    ["rN+"] = "negative energy",
    ["rPois"] = "poison"
  }
  for inscription,res in pairs(resistances) do
    if annot:find(inscription, 1, true) then
      annot = annot .. " {resist " .. res .. "} {" .. res .. " resistance}"
    end
  end

  return annot
end

--- If you want dumps (.lst files) to be annotated, uncomment this line:
-- ch_stash_dump_annotate_item = ch_stash_search_annotate_item
