---------------------------------------------------------------------------
-- wield.lua
-- Selects extra items to wield.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/wield.lua
---------------------------------------------------------------------------

function make_hash(ls)
    local h = { }
    for _, i in ipairs(ls) do
        h[i] = true
    end
    return h
end

function ch_item_wieldable(it)
    -- We only need to check for unusual cases - basic matching is handled
    -- by Crawl itself.
    local spells = make_hash( you.spells() )

    if spells["Bone Shards"]
            and string.find( it.name("a"), " skeleton" )
    then
        return true
    end

    if (spells["Sublimation of Blood"] or spells["Simulacrum"])
            and food.ischunk(it)
    then
        return true
    end

    if spells["Sublimation of Blood"]
            and (string.find( it.name("a"), " potions? of blood") or
                 string.find( it.name("a"), " potions? of coagulated blood"))
    then
        return true
    end

    if spells["Sandblast"]
            and it.class(true) == "missile"
            and string.find( it.name("a"), " stones?" )
    then
        return true
    end

    if spells["Sticks to Snakes"]
            and it.class(true) == "missile"
            and string.find( it.name("a"), " arrows?" )
    then
        return true
    end

    return false
end
