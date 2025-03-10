------------------------------------------------------------------------------
-- ghost.lua
-- Functions for ghost vaults.
------------------------------------------------------------------------------

crawl_require("dlua/dungeon.lua")

-- Integer value from 0 to 100 giving the chance that a ghost-allowing level
-- will attempt to place a ghost vault.
_GHOST_CHANCE_PERCENT = 10

--[[
This function should be called by every ghost vault. It sets the common tags
needed for ghost vaults and sets the common ghost chance for vaults not placing
in Vaults branch. Vaults placing in the Vaults branch need to have the tag
`vaults_ghost`, which is not set by this function. See the ghost vault
guidelines in docs/develop/levels/guidelines.md.

@bool[opt=false] vaults_setup If true, this vault will place in the Vaults
    branch, so add the `vaults_ghost` tag and replace all `c` glyphs with `x`.
@bool[opt=true] set_chance If true, set the ghost vault chance. Only vaults
    that place exclusively in the Vaults branch should set this to false, as
    they should not set a CHANCE and only use the `vaults_ghost` tag.
]]
function ghost_setup(e, vaults_setup, set_chance)
    if set_chance == nil then
        set_chance = true
    end

    -- Tags common to all ghost vaults.
    e.tags("luniq_player_ghost no_tele_into no_trap_gen no_monster_gen")

    -- We must set the `vaults_ghost` tag unconditionally so that the vault is
    -- available for selection by that tag. The `c` substitution is only done
    -- if we're actually placing the vault in the Vaults branch.
    if vaults_setup then
        e.tags("vaults_ghost")
        if you.in_branch("Vaults") then
            e.subst("c = x")
        end
    end

    if set_chance then
        e.tags("chance_player_ghost")
        -- Ensure we don't use CHANCE in Vaults.
        e.depth_chance("Vaults", 0)
        e.chance(math.floor(_GHOST_CHANCE_PERCENT / 100 * 10000))
    end
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
-- This function is used by the more challenging ghost vaults that place many
-- additional monsters as a way to make attempting the vault more worthwhile.
--
-- @tparam table e environment
-- @tparam string kglyphs If nil, use 'd' and 'e' item slots. Otherwise, define
--                        KITEMs on the two characters supplied in kglyphs.
function ghost_good_loot(e, kglyphs)

    local item1fn, item2fn
    if kglyphs == nil then
        item1fn = e.item
        item2fn = e.item
    else
        item1fn = function(def) e.kitem(kglyphs:sub(1, 1) .. " = " .. def) end
        item2fn = function(def) e.kitem(kglyphs:sub(2, 2) .. " = " .. def) end
    end

    -- Possible loot items.
    local jewellery = "any jewellery"
    local good_jewellery = "any jewellery good_item"
    local randart_jewellery = "any jewellery randart"
    local aux = dgn.aux_armour

    local first_item = true
    local second_item = false
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
    local first_loot = {
                   {name = "scrolls", def = dgn.loot_scrolls, weight = 20},
                   {name = "potions", def = dgn.loot_potions, weight = 20},
                   {name = "aux", def = aux, weight = 10},
                   {name = "jewellery", def = jewellery, weight = 10},
                   {name = "manual", def = "any manual", weight = 5} }
    local second_loot = {
                    {name = "scrolls", def = dgn.loot_scrolls, weight = 10},
                    {name = "potions", def = dgn.loot_potions, weight = 10} }

    -- If we're upgrading the first item , choose a class, define the item
    -- slot, otherwise the slot becomes the usual '|*' definition.
    if first_item then
        chosen = util.random_weighted_from("weight", first_loot)
        item1fn(chosen["def"])
    else
        item1fn("superb_item / star_item")
    end
    if second_item then
        chosen = util.random_weighted_from("weight", second_loot)
        item2fn(chosen["def"])
    else
        item2fn("superb_item / star_item")
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
    local quality = ""
    local great_weight = 1
    local base_weapons = {}
    local good_weapons = {}
    local great_weapons = {}
    local variability = nil

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
                        "hand axe", "war axe", "trident", "halberd",
                        "quarterstaff"}
        good_weapons = {"morningstar", "dire flail", "broad axe", "partisan"}
        quality = crawl.one_chance_in(4) and "good_item" or ""
    elseif you.absdepth() < 14 then
        base_weapons = {"rapier", "long sword", "scimitar", "flail",
                        "morningstar", "dire flail", "war axe", "broad axe",
                        "trident", "partisan", "halberd", "quarterstaff"}
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
                        "battleaxe", "partisan", "glaive", "quarterstaff"}
        good_weapons = {}
        great_weapons = {"quick blade", "demon blade", "double sword",
                         "triple sword", "demon whip", "eveningstar",
                         "executioner's axe", "demon trident", "bardiche",
                         "lajatang"}
        great_weight = 3
        quality = crawl.coinflip() and "good_item"
                  or crawl.coinflip() and "randart"
                  or ""

        -- if variability has already generated, this will end up with a
        -- chaos branded great mace as a backup.
        variability = crawl.one_chance_in(100) and "mace of Variability"
    end

    -- Make one weapons table with each weapon getting weight by class.
    local weapons = {}
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
                 or dgn.random_item_def(weapons, "chaos", quality, "|")
    e.mons("dancing weapon; " .. weapon_def)
end

-- Set up equipment for the fancier orc warriors, knights, and warlord in
-- biasface_ghost_orc_armoury and biasface_vaults_ghost_orc_armoury.
function setup_armoury_orcs(e)
    weapon_quality = crawl.coinflip() and "randart" or "good_item"
    warrior_def = dgn.monster_weapon("warrior", nil, weapon_quality)
    knight_def = dgn.monster_weapon("knight", nil, weapon_quality)
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
            " . kite shield good_item w:4 | kite shield randart w:2 " ..
            "    | tower shield good_item w:2 | tower shield randart w:1")
end
