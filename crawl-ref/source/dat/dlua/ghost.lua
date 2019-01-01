crawl_require("dlua/dungeon.lua")

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
                   and (not iname:find("quick blade") or ename ~= "speed") then
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

-- Basic loot scale for extra loot for lone ghosts. Takes none_glyph to use
-- when it would like no item and adds depth appropriate items to d and e
-- glyphs.
function lone_ghost_extra_loot(e, none_glyph)
    if you.in_branch("D") then
        if you.depth() < 6 then
            e.subst("de = " .. none_glyph)
        elseif you.depth() < 9 then
            e.subst("d = %$" .. none_glyph .. none_glyph)
            e.subst("e = " ..none_glyph)
        elseif you.absdepth() < 12 then
            e.subst("d = %$")
            e.subst("e = " ..none_glyph)
        elseif you.absdepth() < 14 then
            e.subst("d = |*")
            e.subst("e = %$")
        else
            e.subst("d = |*")
            e.subst("e = *$")
        end
    elseif you.in_branch("Lair") then
        e.subst("d = *%")
        e.subst("e = %$" .. none_glyph .. none_glyph)
    elseif you.in_branch("Orc") then
        e.subst("d = |*")
        e.subst("e = %$")
    else
        e.subst("de = |*")
    end
end

-- Basic loot scale for ghost "guarded" loot for lone ghost vaults. KITEMS this
-- loot to ghost_glyph.
function lone_ghost_guarded_loot(e, ghost_glyph)
    if you.in_branch("D") then
        if you.depth() < 6 then
            e.kitem(ghost_glyph .. " = star_item / any")
        elseif you.depth() < 9 then
            e.kitem(ghost_glyph .. " = star_item")
        else
            e.kitem(ghost_glyph .. " = superb_item / star_item")
        end
    else
        e.kitem(ghost_glyph .. " = superb_item / star_item")
    end
end

-- Loot scaling that gradually upgrades items on glyphs 'd' and 'e' with depth
-- of placement. Starting from D:9, 'd' has a chance to become a good potion or
-- scroll item (according to dgn.loot_potions and dgn.loot_scrolls), an
-- auxiliary armour item, or a jewellery item. The latter two can be good_item
-- and then randart with increasing depth.
--
-- This function is mostly used by the more challenging ghost vaults that place
-- many additional monsters as a way to make attempting the vault more
-- worthwhile.
function ghost_good_loot(e)
    -- Possible loot items.
    jewellery = "any jewellery"
    good_jewellery = "any jewellery good_item"
    randart_jewellery = "any jewellery randart"
    aux = dgn.aux_armour

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
                aux = dgn.good_aux_armour
                jewellery = good_jewellery
            end
            if crawl.one_chance_in(3) then
                second_item = true
            end
        else
            aux = dgn.good_aux_armour
            jewellery = good_jewellery
            if crawl.one_chance_in(4) then
               aux = dgn.randart_aux_armour
               jewellery = randart_jewellery
            end
            if crawl.coinflip() then
                second_item = true
            end
         end
    elseif you.in_branch("Lair") then
        if crawl.one_chance_in(3) then
            aux = dgn.good_aux_armour
            jewellery = good_jewellery
        end
        if crawl.one_chance_in(4) then
            second_item = true
        end
    elseif you.in_branch("Orc") then
        if crawl.coinflip() then
            aux = dgn.good_aux_armour
            jewellery = good_jewellery
        end
        if crawl.one_chance_in(3) then
            second_item = true
        end
    elseif you.in_branch("Shoals")
      or you.in_branch("Snake")
      or you.in_branch("Spider")
      or you.in_branch("Swamp") then
        aux = dgn.good_aux_armour
        jewellery = good_jewellery
        if crawl.one_chance_in(3) then
           aux = dgn.randart_aux_armour
           jewellery = randart_jewellery
        end
        second_item = true
    elseif you.in_branch("Vaults") or you.in_branch("Elf") then
        aux = dgn.good_aux_armour
        jewellery = good_jewellery
        if crawl.coinflip() then
           aux = dgn.randart_aux_armour
           jewellery = randart_jewellery
        end
        second_item = true
    else
        aux = dgn.randart_aux_armour
        jewellery = randart_jewellery
        second_item = true
    end

    -- Define loot tables of potential item defs.
    first_loot = { {name = "scrolls", def = dgn.loot_scrolls, weight = 20},
                   {name = "potions", def = dgn.loot_potions, weight = 20},
                   {name = "aux", def = aux, weight = 10},
                   {name = "jewellery", def = jewellery, weight = 10},
                   {name = "manual", def = "any manual", weight = 5} }
    second_loot = { {name = "scrolls", def = dgn.loot_scrolls, weight = 10},
                    {name = "potions", def = dgn.loot_potions, weight = 10} }

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

-- Determine the number of gold piles placed for ebering_ghost_gozag and
-- ebering_vaults_ghost_gozag. Mean ranges from 3 to 15 from D:3-D:15 with
-- 12-13 in Orc and 15 elsewhere, and the actual number placed varies by +/- 3.
function setup_gozag_gold(e)
    if you.in_branch("D") then
        depth = you.depth()
    elseif you.in_branch("Orc") then
        depth = 11 + you.depth()
    else
        depth = you.absdepth()
    end
    pile_mean = math.min(15, math.floor(3/4 * depth + 3.75))
    pile_count = crawl.random_range(pile_mean - 3, pile_mean + 3)
    e.nsubst("' = " .. pile_count .. "=$ / -")
end

-- Set up the chaos dancing weapon for ebering_ghost_xom and
-- ebering_vaults_ghost_xom.
function setup_xom_dancing_weapon(e)
    quality = ""
    great_weight = 1
    great_weapons = {}

    -- Progress the base type of weapons so that it's more reasonable as reward
    -- as depth increases. The very rare weapons only show up at all in later
    -- depths and always with lower weight.
    if you.absdepth() < 9 then
        base_weapons = {"dagger", "short sword", "rapier", "falchion",
                        "long sword", "whip", "mace", "hand axe", "spear",
                        "trident"}
        good_weapons = {"scimitar", "flail", "war axe", "quarterstaff"}
        quality = crawl.one_chance_in(6) and "good_item" or ""
    elseif you.absdepth() < 12 then
        base_weapons = {"rapier", "long sword", "scimitar", "mace", "flail",
                        "hand axe", "war axe", "trident", "quarterstaff"}
        good_weapons = {"morningstar", "dire flail", "broad axe", "halberd"}
        quality = crawl.one_chance_in(4) and "good_item" or ""
    elseif you.absdepth() < 14 then
        base_weapons = {"rapier", "long sword", "scimitar", "flail",
                        "morningstar", "dire flail", "war axe", "broad axe",
                        "trident", "halberd", "scythe", "quarterstaff"}
        good_weapons = {"great sword", "great mace", "battleaxe", "glaive"}
        great_weapons = {"quick blade", "demon blade", "double sword",
                         "triple sword", "demon whip", "eveningstar",
                         "executioner's axe", "demon trident", "bardiche",
                         "lajatang"}
        quality = crawl.one_chance_in(3) and "good_item"
                  or crawl.one_chance_in(4) and "randart"
                  or ""
    else
        base_weapons = {"rapier", "scimitar", "great sword", "morningstar",
                        "dire flail", "great mace", "war axe", "broad axe",
                        "battleaxe", "halberd", "scythe", "glaive",
                        "quarterstaff"}
        good_weapons = {}
        great_weapons = {"quick blade", "demon blade", "double sword",
                         "triple sword", "demon whip", "eveningstar",
                         "executioner's axe", "demon trident", "bardiche",
                         "lajatang"}
        great_weight = 3
        quality = crawl.coinflip() and "good_item"
                  or crawl.coinflip() and "randart"
                  or ""
        variability = not you.unrands("mace of Variability")
                      and crawl.one_chance_in(100) and "mace of Variability"
    end

    -- Make one weapons table with each weapon getting weight by class.
    weapons = {}
    for _, wname in ipairs(base_weapons) do
        weapons[wname] = 10
    end
    for _, wname in ipairs(good_weapons) do
        weapons[wname] = 5
    end
    for _, wname in ipairs(great_weapons) do
        weapons[wname] = great_weight
    end

    -- Generate a dancing weapon based on the table that always has chaos ego.
    weapon_def = variability
                 or random_item_def(weapons, {["chaos"] = 1}, quality, "|")
    e.mons("dancing weapon; " .. weapon_def)
end

-- Some melee weapons sets for warrior monsters. Loosely based on the orc
-- warrior and knight sets, but including a couple more types and some of the
-- high-end weapons at reasonable weights.
ghost_warrior_weap = {
    ["short sword"] = 5, ["rapier"] = 10, ["long sword"] = 10,
    ["scimitar"] = 5, ["great sword"] = 10, ["hand axe"] = 5, ["war axe"] = 10,
    ["broad axe"] = 5, ["battleaxe"] = 10, ["spear"] = 5, ["trident"] = 10,
    ["halberd"] = 10, ["glaive"] = 5, ["whip"] = 5, ["mace"] = 10,
    ["flail"] = 10, ["morningstar"] = 5, ["dire flail"] = 10,
    ["great mace"] = 5, ["quarterstaff"] = 10
}
ghost_knight_weap = {
    ["scimitar"] = 15, ["demon blade"] = 5, ["double sword"] = 5,
    ["great sword"] = 15, ["triple sword"] = 5, ["war axe"] = 5,
    ["broad axe"] = 10, ["battleaxe"] = 15, ["executioner's axe"] = 5,
    ["demon trident"] = 5, ["glaive"] = 10, ["bardiche"] = 5,
    ["morningstar"] = 10, ["demon whip"] = 5, ["eveningstar"] = 5,
    ["dire flail"] = 10, ["great mace"] = 10, ["lajatang"] = 5
}

-- Set up equipment for the fancier orc warriors, knights, and warlord in
-- biasface_ghost_orc_armoury and biasface_vaults_ghost_orc_armoury.
function setup_armoury_orcs(e)
    weapon_quality = crawl.coinflip() and "randart" or "good_item"
    warrior_def = random_item_def(ghost_warrior_weap, nil, weapon_quality, '|')
    knight_def = random_item_def(ghost_knight_weap, nil, weapon_quality, '|')
    e.kmons("D = orc warrior ; " .. warrior_def .. " . chain mail good_item " ..
            "    | chain mail randart | plate armour good_item" ..
            "    | plate armour randart")
    e.kmons("E = orc knight ; " .. knight_def .. " . chain mail good_item " ..
            "    | chain mail randart | plate armour good_item " ..
            "    | plate armour randart")
    e.kmons("F = orc warlord ; " .. knight_def ..
            " . chain mail good_item " ..
            "    | chain mail randart | plate armour good_item " ..
            "    | plate armour randart " ..
            " . shield good_item w:4 | shield randart w:2 " ..
            "    | large shield good_item w:2 | large shield randart w:1")
end
