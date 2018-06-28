-- Integer value from 0 to 100 giving the chance that a ghost-allowing level
-- will attempt to place a ghost vault.
_GHOST_CHANCE_PERCENT = 10

-- Common setup we want regardless of the branch of the ghost vault.
function ghost_setup_common(e)
     e.tags("allow_dup luniq_player_ghost")
     e.tags("no_tele_into no_trap_gen no_monster_gen")
end

-- For vaults that want more control over their tags, use this function to
-- properly set the common chance value and tag.
function set_ghost_chance(e)
    e.tags("chance_player_ghost")
    e.chance(math.floor(_GHOST_CHANCE_PERCENT / 100 * 10000))
end

-- This function should be called in every ghost vault that doesn't place in
-- the Vaults branch. It sets the minimal tags we want as well as the common
-- ghost chance. If you want your vault to be less common, use a WEIGHT
-- statement; don't set a difference CHANCE from the one here.
function ghost_setup(e)
    ghost_setup_common(e)

    set_ghost_chance(e)
end

-- Every Vaults room that places a ghost should call this function.
function vaults_ghost_setup(e)
    ghost_setup_common(e)

    -- Vaults branch layout expects vaults_ghost as a tag selector.
    e.tags("vaults_ghost")
end

-- Make an item definition that will randomly choose from combinations of the
-- given tables of weighted item types and optional egos.
--
-- @param items     A table with weapon names as keys and weights as values.
-- @param egos      An optional table with ego names as keys and weights as
--                  values.
-- @param args      An optional string of arguments to use on every item entry.
--                  Should not have leading or trailing whitespace.
-- @param separator An optional separator to use between the item entries.
--                  Defaults to '/', which is appropriate for ITEM statements.
--                  Use '|' if making item statements for use with MONS.
-- @returns A string containing the item definition.
function random_item_def(items, egos, args, separator)
    args = args ~= nil and " " .. args or ""
    separator = separator ~= nil and separator or '/'
    local item_def

    for iname, iweight in pairs(items) do
        -- If we have egos, define an item spec with all item+ego
        -- combinations, each with weight scaled by item rarity and ego
        -- rarity.
        if egos ~= nil then
            for ename, eweight in pairs(egos) do
                if (not iname:find("demon") or ename ~= "holy_wrath")
                   and (not make_arte or ename ~= "none") then
                    def = iname .. args .. " ego:" .. ename .. " w:" ..
                          math.floor(iweight * eweight)
                    if item_def == nil then
                         item_def = def
                    else
                         item_def = item_def .. " " .. separator .. " " .. def
                    end
                end
            end
        -- No egos, so define item spec with all item combinations, each with
        -- weight scaled by item rarity.
        else
            def = iname .. args .. " w:" .. iweight
            if item_def == nil then
                 item_def = def
            else
                 item_def = item_def .. " " .. separator .. " " .. def
            end
        end
    end
    return item_def
end

-- Some scroll and potions with weights that are used as nice loot.
ghost_loot_scrolls = "scroll of teleportation w:15 / scroll of fog w:15 / " ..
    "scroll of fear w:15 / scroll of blinking w:10 / " ..
    "scroll of enchant weapon w:5 / scroll of enchant armour w:5 / " ..
    "scroll of brand weapon w:3 / scroll of magic mapping w:10 / " ..
    "scroll of acquirement w:1"
ghost_loot_potions = "potion of haste w:15 / potion of might w:10 / " ..
    "potion of invisibility w:10 / potion of agility w:10 / " ..
    "potion of brilliance w:5 / potion of magic w:10 / " ..
    "potion of heal wounds w:15 / potion of mutation w:10 / " ..
    "potion of cancellation w:10 / potion of resistance w:5 / " ..
    "potion of experience w:1"

function ghost_good_loot(e)
    -- Possible loot items.
    jewellery = "any jewellery"
    good_jewellery = "any jewellery good_item"
    randart_jewellery = "any jewellery randart"
    aux = "cloak / scarf / helmet / hat / gloves / pair of boots"
    good_aux = "cloak good_item / scarf good_item / helmet good_item / " ..
        "hat good_item / gloves good_item / pair of boots good_item"
    randart_aux = good_aux:gsub("good_item", "randart")

    first_item = true
    second_item = false
    if you.in_branch("D") then
        if you.depth() < 9 then
            first_item = false
        elseif you.depth() < 12 then
            if crawl.coinflip() then
                first_item = false
            end
        elseif you.depth() < 14 then
            if crawl.coinflip() then
                aux = good_aux
                jewellery = good_jewellery
            end
            if crawl.one_chance_in(3) then
                second_item = true
            end
        else
            aux = good_aux
            jewellery = good_jewellery
            if crawl.one_chance_in(4) then
               aux = randart_aux
               jewellery = randart_jewellery
            end
            if crawl.coinflip() then
                second_item = true
            end
         end
    elseif you.in_branch("Lair") then
        if crawl.one_chance_in(3) then
            aux = good_aux
            jewellery = good_jewellery
        end
        if crawl.one_chance_in(4) then
            second_item = true
        end
    elseif you.in_branch("Orc") then
        if crawl.coinflip() then
            aux = good_aux
            jewellery = good_jewellery
        end
        if crawl.one_chance_in(3) then
            second_item = true
        end
    elseif you.in_branch("Shoals")
      or you.in_branch("Snake")
      or you.in_branch("Spider")
      or you.in_branch("Swamp") then
        aux = good_aux
        jewellery = good_jewellery
        if crawl.one_chance_in(3) then
           aux = randart_aux
           jewellery = randart_jewellery
        end
        second_item = true
    elseif you.in_branch("Vaults") or you.in_branch("Elf") then
        aux = good_aux
        jewellery = good_jewellery
        if crawl.coinflip() then
           aux = randart_aux
           jewellery = randart_jewellery
        end
        second_item = true
    else
        aux = randart_aux
        jewellery = randart_jewellery
        second_item = true
    end

    -- Define loot tables of potential item defs.
    first_loot = { {name = "scrolls", def = ghost_loot_scrolls, weight = 20},
                   {name = "potions", def = ghost_loot_potions, weight = 20},
                   {name = "aux", def = aux, weight = 10},
                   {name = "jewellery", def = jewellery, weight = 10},
                   {name = "manual", def = "any manual", weight = 5} }
    second_loot = { {name = "scrolls", def = ghost_loot_scrolls, weight = 10},
                    {name = "potions", def = ghost_loot_potions, weight = 10} }

    -- If we're upgrading the first item , choose a class, define the item
    -- slot, otherwise the slot becomes the usual '|*' definition.
    if first_item then
        chosen = util.random_weighted_from("weight", first_loot)
        e.item(chosen["def"])
    else
        e.item("superb_item / star_item")
    end
    if second_item then
        chosen = util.random_weighted_from("weight", second_loot)
        e.item(chosen["def"])
    else
        e.item("superb_item / star_item")
    end
end
