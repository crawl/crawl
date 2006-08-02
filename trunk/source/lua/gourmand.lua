-- SPOILER WARNING
--
-- This file contains spoiler information. Do not read or use this file if you
-- don't want to be spoiled. Further, the Lua code in this file may use this
-- spoily information to take actions on your behalf. If you don't want
-- automatic exploitation of spoilers, don't use this.
-- 
---------------------------------------------------------------------------
-- gourmand.lua: (requires eat.lua, chnkdata.lua, and safechunk.lua)
-- 
-- Eats all available chunks on current square and inventory, swapping to an
-- identified amulet of the gourmand if necessary, with no prompts. Note that
-- it relies on spoiler information to identify chunks it can eat without
-- prompting the user.
--
-- This is a Lua macro, so the action will be interrupted by the hp/stat loss,
-- or monsters.
--
-- To use this, add these line to your init.txt:
--   lua_file = lua/gourmand.lua
--
-- And macro the "eat_gourmand" function to a key.
---------------------------------------------------------------------------
-- Macroable
function eat_gourmand()
    local race = you.race()
    local all = { }
    for _, it in ipairs(you.floor_items()) do table.insert(all, it) end
    for _, it in ipairs(item.inventory())  do table.insert(all, it) end

    local chunks = { }
    local needgourmand = false
    local oneedible = false

    for _, itym in ipairs(all) do
        if food.ischunk(itym) then
            table.insert(chunks, itym)
        end
    end

    for _, itym in ipairs(chunks) do
        local rot = food.rotting(itym)
        local mon = chunkmon(itym)
        
        if food.can_eat(itym) and sc_safechunk and 
                sc_safechunk(rot, race, mon) then
            oneedible = true
        end
        
        -- If we can't eat it now, but we could eat it if hungry, a gourmand
        -- switch would be nice.
        if not food.can_eat(itym) and food.can_eat(itym, false) then
            needgourmand = true
        end

        if sc_safechunk and not sc_safechunk(rot, race, mon)
            and sc_safechunk(false, race, mon)
        then
            needgourmand = true
        end
    end

    if table.getn(chunks) == 0 then
        crawl.mpr("No chunks to eat.")
        return
    end

    local amuremoved
    if needgourmand and not oneedible then
        amuremoved = switch_amulet("gourmand")
    end

    for _, ch in ipairs(chunks) do
        if food.can_eat(ch) and is_chunk_safe(ch) then
            while item.quantity(ch) > 0 do
                if food.eat(ch) then
                    coroutine.yield(true)
                else
                    break
                end
            end
        end
    end

    if amuremoved then
        switch_amulet(amuremoved)
    end
end

function chunkmon(chunk)
    local cname = item.name(chunk)
    local mon
    _, _, mon = string.find(cname, "chunk of (.+) flesh")
    return mon
end

function switch_amulet(amu)
    local towear
    if type(amu) == "string" then
        for _, it in ipairs(item.inventory()) do
            if item.class(it, true) == "jewelry" 
                    and item.subtype(it) == amu
                    and not item.cursed(it) then
                towear = item.slot(it)
                break
            end
        end

        if not towear then
            crawl.mpr("Can't find suitable amulet: " .. amu)
            coroutine.yield(false)
        end
    else
        towear = amu
    end

    local curramu = item.equipped_at("amulet")
    if curramu and item.cursed(curramu) then
        crawl.mpr("Can't swap out cursed amulet!")
        coroutine.yield(false)
    end

    local wearitem = item.inslot(towear)

    if curramu then
        if not item.remove(curramu) then
            coroutine.yield(false)
        end

        -- Yield, so that a turn can pass and we can take another action.
        coroutine.yield(true)
    end

    if not item.puton(wearitem) then
        coroutine.yield(false)
    end

    coroutine.yield(true)
    return curramu and item.slot(curramu)
end

function c_interrupt_macro(interrupt_name)
    return interrupt_name == "hp_loss" or interrupt_name == "stat" or
           interrupt_name == "monster" or interrupt_name == "force"
end
