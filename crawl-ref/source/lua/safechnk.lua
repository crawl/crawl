-- SPOILER WARNING
--
-- This file contains spoiler information. Do not read or use this file if you
-- don't want to be spoiled. Further, the Lua code in this file may use this
-- spoily information to take actions on your behalf. If you don't want
-- automatic exploitation of spoilers, don't use this.
-- 
---------------------------------------------------------------------------
-- safechunk.lua:
-- Determines whether a chunk of meat is safe for your character to eat.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/safechunk.lua
--
-- This file has no directly usable functions, but is needed by gourmand.lua
-- and enables auto_eat_chunks in eat.lua.
---------------------------------------------------------------------------

function sc_safechunk(rot, race, mon)
    if race == "Ghoul" then
        return true
    end

    if rot then
        if race ~= "Kobold" and race ~= "Troll" then
            return false
        end
    end

    if sc_pois[mon] and you.res_poison() > 0 then
        return true
    end

    if sc_hcl[mon] or sc_mut[mon] then
        return false
    end

    -- Only contaminated and clean chunks remain, in theory. We'll accept
    -- either
    return true
end
