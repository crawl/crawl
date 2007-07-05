---------------------------------------------------------------------------
-- stash.lua
-- Annotates items for the stash-tracker's search.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/stash.lua
--
---------------------------------------------------------------------------

-- Annotate items for searches
function ch_stash_search_annotate_item(it)
    local annot = ""

    if item.artifact(it) then
        annot = annot .. "{artefact} "
    elseif item.branded(it) then
        annot = annot .. "{ego} "
    elseif item.class(it, true) == "book" then
        annot = annot .. "{book} "
    end

    local skill = item.weap_skill(it)
    if skill then
        annot = annot .. "{" .. skill .. "} "
    end

    return annot
end

--- If you want dumps (.lst files) to be annotated, uncomment this line:
-- ch_stash_dump_annotate_item = ch_stash_search_annotate_item

