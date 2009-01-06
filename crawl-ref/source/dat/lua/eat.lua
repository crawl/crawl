---------------------------------------------------------------------------
-- eat.lua:
-- Prompts to eat food in the following order:
--   1) for chunks on the floor *and* in inventory, sorted by age
--   2) for non-chunks on the floor
--   3) for non-chunks in inventory
--   4) opens the food menu of your inventory
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/eat.lua
--
-- See c_eat in this file if you want to tweak eating behaviour further.
---------------------------------------------------------------------------

-- Called by Crawl. Note that once Crawl sees a c_eat function, it bypasses the
-- built-in (e)at command altogether.
--
function c_eat()
    -- Prompt to eat chunks off floor/inventory, sorted by age.
    -- Returns true if the player chose to eat a chunk.
    if food.prompt_eat_chunks() then
        return
    end

    -- Prompt to eat a non-chunk off the floor. Returns true if the player
    -- ate something.
    if food.prompt_floor() then
        return
    end

    -- Prompt to eat a non-chunk from the inventory. Returns true if the player
    -- ate something.
    if food.prompt_inventory() then
        return
    end

    -- Allow the player to choose a snack from inventory.
    food.prompt_inv_menu()
end
