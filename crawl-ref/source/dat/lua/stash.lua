---------------------------------------------------------------------------
-- stash.lua
-- Annotates items for the stash-tracker's search, and for autopickup
-- exception matches.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/stash.lua
--
-- Available annotations:
-- {artefact} for artefacts.
-- {ego} for identified branded items.
-- { <skill> } - the relevant weapon skill for weapons.
-- { <class> } - item class: gold, weapon, missile, armour, wand, carrion,
--               food, scroll, jewellery, potion, book, staff, orb, misc
-- {stick} for items suitable for "sticks to snakes"
-- {fruit} for fruit
--
-- You can optionally disable annotate items with the item class name
-- (such as "weapon" for weapons) by setting
--   annotate_item_class = false
-- in your init.txt.
--
-- Item annotations are always prefixed to the item name. For instance:
-- {artefact} the Staff of Wucad Mu
---------------------------------------------------------------------------

local ch_annotate_item_class   = nil
local ch_annotate_item_dropped = nil

-- Annotate items for searches
function ch_stash_search_annotate_item(it)
  local annot = ""

  if ch_annotate_item_dropped == nil then
    ch_annotate_item_dropped = opt_boolean("annotate_item_dropped", false)
  end

  if ch_annotate_item_dropped and it.dropped then
    annot = annot .. "{dropped} "
  end

  if it.artefact then
    annot = annot .. "{artefact} "
  elseif it.branded then
    annot = annot .. "{ego} "
  end

  if it.snakable then
    annot = annot .. "{stick} "
  end

  if food.isfruit(it) then
    annot = annot .. "{fruit} "
  end

  local skill = it.weap_skill
  if skill then
    annot = annot .. "{" .. skill .. "} "
  end

  if ch_annotate_item_class == nil then
    ch_annotate_item_class = opt_boolean("annotate_item_class", true)
  end

  if ch_annotate_item_class then
    annot = annot .. "{" .. it.class(true) .. "}"
  end

  return annot
end

--- If you want dumps (.lst files) to be annotated, uncomment this line:
-- ch_stash_dump_annotate_item = ch_stash_search_annotate_item
