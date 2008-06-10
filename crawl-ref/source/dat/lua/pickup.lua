---------------------------------------------------------------------------
-- eat.lua:
-- Pick up a butchering weapon if we don't already have one.
-- This requires ) to be in the autopickup option line.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/pickup.lua
---------------------------------------------------------------------------
function can_butcher(it)
    if item.name(it):find("distort", 0, true) then
        return false
    end

    local skill = item.weap_skill(it)
    -- Have to handle polearms separately, since only some of them can butcher.
    if skill == "Polearms" or skill == "Staves" then
        local butcherable_polearms = {
            "scythe", "lajatang", "halberd", "bardiche", "glaive"
        }

        for _, weap in ipairs(butcherable_polearms) do
            if string.find( item.name(it, "a"), weap ) then
               return true 
            end
        end
        return false
    else
        return string.find( skill, "Blades" ) or skill == "Axes"
    end
end

function ch_autopickup(it)
    if item.class(it, true) == "weapon" then

        -- Trolls and Ghouls don't need weapons to butcher things, and Mummies
        -- and Spriggans can't eat chunks. Ideally, we could detect a player
        -- with the claws mutation here too, but that's not currently possible.
        if you.race() == "Troll"
           or you.race() == "Ghoul"
           or you.race() == "Mummy"
           or you.race() == "Spriggan" then
            return false
        end

        -- The item is not a good butchering tool, either.
        if item.cursed(it) or not can_butcher(it) then
            return false
        end

        -- Check the inventory for butchering tools.
        local inv = item.inventory()
        for _, inv_it in ipairs(inv) do
            if item.class(inv_it, true) == "weapon"
               and can_butcher(inv_it) 
               and not item.cursed(inv_it) then
                return false
            end
        end
    end

    -- If we got here, we found no butchering tool in the inventory
    -- AND this weapon is a good candidate tool for a butchering tool.
    -- Ergo: pick it up.
    return true
end