---------------------------------------------------------------------------
-- pickup.lua:
-- Pick up a butchering weapon if we don't already have one.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/pickup.lua
---------------------------------------------------------------------------
local function can_butcher(it, name)
    -- Item can't be used to cut meat.
    if not item.can_cut_meat(it) then
        return false
    end

    -- If we're already wielding a weapon capable of butchering, okay.
    if item.equipped(it) then
        return true
    end

    -- Don't make the user wield a known cursed weapon.
    if item.cursed(it) then
        return false
    end

    -- Nor a known distortion weapon.
    if name:find("distort", 0, true) then
        return false
    end

    -- Else we're good.
    return true
end

function pickup_butcher(it, name)
    -- If you can butcher with your claws you don't need a butchering tool.
    if you.has_claws() > 0 then
        return false
    end

    -- Same if you don't ever need to butcher corpses.
    if not you.can_consume_corpses() and not you.god_likes_butchery() then
        return false
    end

    -- Can this item even be used for butchering?
    if not can_butcher(it, name) then
        return false
    end

    -- Do we already have a butchering tool?
    for _, inv_it in pairs(item.inventory()) do
        if can_butcher(inv_it, item.name(inv_it)) then
            return false
        end
    end
    return true
 end

add_autopickup_func(pickup_butcher)
