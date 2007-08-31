------------------------------------------------------------
-- pickup.lua:
-- Smarter autopickup handling that takes into account the 
-- item type in combination with your character's race, 
-- religion, knowledge, and eating habits. 
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/pickup.lua
--
-- Notes:
--  * This script only handles items of classes with 
--    autopickup on.
--  * Any result can still be overridden using 
--    autopickup_exceptions.
------------------------------------------------------------

function make_hash(ls)
    local h = { }
    for _, i in ipairs(ls) do
        h[i] = true
    end
    return h
end

function you_undead()
    return you.race() == "Mummy" or you.race() == "Ghoul" 
           or you.race() == "Vampire"
end

-- not identified
function unknown_potion(type)
    return type == 0
end

function good_potion(type)
    return type == 1
end

function bad_potion(type)
    return type == 2
end

-- special cases
function spec_potion(type)
    return type == 3
end

function ch_autopickup(it)
    local spells = make_hash( you.spells() )

    if item.class(it) == "Potions" then
   
        local type = item.potion_type(it)

        -- "bad" potions only for Evaporate
        if spells["Evaporate"] and bad_potion(type) then
           return true
        end

        -- no potions for Mummies
	-- also: no bad potions for anyone else
        if you.race() == "Mummy" or bad_potion(type) then
           return false
        end

        -- pickup "good" and unID'd potions
        if good_potion(type) or unknown_potion(type) then
           return true
        end

        -- special handling
        if spec_potion(type) then

           -- undead cannot use berserk
	   -- or anything involving mutations
           if item.subtype(it) == "berserk" 
	        or item.subtype(it) == "gain ability"
		or item.subtype(it) == "cure mutation" then
              if you_undead() then
                 return false
              else 
                 return true
              end
           end

           -- special cases for blood, water, and porridge
           if item.subtype(it) == "blood" 
	        or item.subtype(it) == "water" 
                or item.subtype(it) == "porridge" then
              return food.can_eat(it, false)
           end
        end

        -- anything not handled until here can be picked up
        return true
    end

    if item.class(it) == "Carrion" 
         or item.class(it) == "Comestibles" then
       return food.can_eat(it, false)
    end

    if item.class(it) == "Books" and you.god() == "Trog"
       then return false
    end

    if item.class(it) == "Jewellery" then
       if item.subtype(it) == "hunger" 
            or item.subtype(it) == "inaccuracy" then
          return false
       end 
       if you_undead() and
            (item.subtype(it) == "regeneration" 
	       or item.subtype(it) == "rage"
	       or item.subtype(it) == "sustenance"
	          and you.race() == "Mummy") then
          return false
       end 
    end
       
    -- we only get here if class autopickup ON
    return true
end

