---------------------------------------------------------------------------
-- eat.lua:
-- Prompts to eat chunks from inventory.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/eat.lua
--
-- See c_eat in this file if you want to tweak eating behaviour further.
---------------------------------------------------------------------------
function prompt_eat(i)
    local iname = item.name(i, "a")
    if item.quantity(i) > 1 then
        iname = "one of " .. iname
    end
    crawl.mpr("Eat " .. iname .. "?", "prompt")

    local res
    res = crawl.getch()
    if res == 27 then
        res = "escape"
    elseif res < 32 or res > 127 then
        res = ""
    else
        res = string.lower(string.format("%c", res))
    end
    return res
end

function is_chunk_safe(chunk)
    local rot = food.rotting(chunk)
    local race = you.race()

    -- Check if the user has sourced safechunk.lua and chnkdata.lua
    if not (sc_cont and sc_pois and sc_hcl and sc_mut and sc_safechunk) then
        return false
    end
    
    local cname = item.name(chunk)
    local mon
    _, _, mon = string.find(cname, "chunk of (.+) flesh")
    
    return sc_safechunk(rot, race, mon)
end

-- Called by Crawl. Note that once Crawl sees a c_eat function, it bypasses the
-- built-in (e)at command altogether.
--
function c_eat(floor, inv)
    -- To enable auto_eat_chunks, you also need to source chnkdata.lua and
    -- safechunk.lua. WARNING: These files contain spoily information.
    local auto_eat_chunks = options.auto_eat_chunks == "yes" or
                            options.auto_eat_chunks == "true"
                            
    if auto_eat_chunks then
        local all = { }
        for _, it in ipairs(floor) do table.insert(all, it) end
        for _, it in ipairs(inv)   do table.insert(all, it) end

        for _, it in ipairs(all) do
            if food.ischunk(it) and food.can_eat(it) and is_chunk_safe(it) then
                local iname = item.name(it, "a")
                if item.quantity(it) > 1 then
                    iname = "one of " .. iname
                end
                crawl.mpr("Eating " .. iname)
                food.eat(it)
                return
            end
        end
    end

    -- Prompt to eat chunks off the floor. Returns true if the player chose
    -- to eat a chunk.
    if food.prompt_floor() then
        return
    end
    
    for i, it in ipairs(inv) do
        -- If we have chunks in inventory that the player can eat, prompt.
        if food.ischunk(it) and food.can_eat(it) then
            local answer = prompt_eat(it)
            if answer == "q" then
                break
            end
            if answer == "escape" then
                return
            end
            if answer == "y" then
                food.eat(it)
                return
            end
        end
    end

    -- Allow the player to choose a snack from inventory
    food.prompt_inventory()
end
