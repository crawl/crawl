---------------------------------------------------------------------------
-- wield.lua
-- Selects extra items to wield.
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
    local spells = make_hash(you.spells())

    if spells["Sublimation of Blood"] and food.ischunk(it)
            or spells["Simulacrum"] and food.ismeaty(it)
    then
        return true
    end

    if spells["Sublimation of Blood"]
            and (string.find(it.name("a"), " potions? of blood") or
                 string.find(it.name("a"), " potions? of coagulated blood"))
    then
        return true
    end

    if spells["Sandblast"]
            and it.class(true) == "missile"
            and (string.find(it.name("a"), " stones?")
                 or string.find(it.name("a"), " large rocks?")
                    and (you.race() == "Troll" or you.race() == "Ogre"))
    then
        return true
    end

    if spells["Sticks to Snakes"]
            and it.class(true) == "missile"
            and string.find(it.name("a"), " arrows?")
    then
        return true
    end

    return false
end
