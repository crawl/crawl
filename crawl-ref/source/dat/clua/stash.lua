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
-- {stick} for items suitable for "sticks to snakes"
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

  if it.is_throwable then
    annot = annot .. "{throwable} "
  end

  if it.artefact then
    annot = annot .. "{artefact} {artifact} "
  elseif it.branded then
    annot = annot .. "{ego} {branded} "
  end

  if it.snakable then
    annot = annot .. "{stick} "
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
      annot = annot .. "{" .. it.ego_type_terse .. "} "
  end

  if it.class(true) == "armour" then
      annot = annot .. "{" .. it.subtype() .. " "
  else
      annot = annot .. "{"
  end
  annot = annot .. it.class(true) .. "}"

  return annot
end

--- If you want dumps (.lst files) to be annotated, uncomment this line:
-- ch_stash_dump_annotate_item = ch_stash_search_annotate_item
