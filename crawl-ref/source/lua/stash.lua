---------------------------------------------------------------------------
-- stash.lua
-- Annotates items for the stash-tracker's search, and for autopickup
-- exception matches.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/stash.lua
--
-- Available annotations:
-- {artefact} for identified artefacs.
-- {ego} for identified branded items.
-- { <skill> } - the relevant weapon skill for weapons.
--
-- You can optionally annotate items with the item class name (such as
-- "weapon" for weapons) by setting
--   annotate_item_class = true
-- in your init.txt.
--
-- The full list of item class names is:
-- gold, weapon, missile, armour, wand, food, scroll, jewelry, potion,
-- book, staff, orb, misc, carrion
--
-- Item annotations are always prefixed to the item name. For instance:
-- {artefact} the Staff of Wucad Mu
---------------------------------------------------------------------------

local ch_annotate_item_class = nil
-- Annotate items for searches
function ch_stash_search_annotate_item(it)
  local annot = ""
  
  if item.artefact(it) then
    annot = annot .. "{artefact} "
  elseif item.branded(it) then
    annot = annot .. "{ego} "
  end

  local skill = item.weap_skill(it)
  if skill then
    annot = annot .. "{" .. skill .. "} "
  end

  if ch_annotate_item_class == nil then
    ch_annotate_item_class = opt_boolean("annotate_item_class")
  end
  
  if ch_annotate_item_class then
    annot = annot .. "{" .. item.class(it, true) .. "}"
  end

  return annot
end

--- If you want dumps (.lst files) to be annotated, uncomment this line:
-- ch_stash_dump_annotate_item = ch_stash_search_annotate_item

