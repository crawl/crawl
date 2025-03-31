------------------------------------------------------------------------------
-- ghost.lua
-- Functions for ghost vaults.
------------------------------------------------------------------------------

crawl_require("dlua/dungeon.lua")

-- Set up the non-flaming edged weapon "left" by the ghost in
-- gammafunk_ghost_hydra_chop.
function setup_hydra_weapon(e)
    -- Possible weapon types with weights. We stick to weapons wieldable by all
    -- non-felids.
    local weapons = {["long sword"] = 8, ["scimitar"] = 12,
                     ["demon blade"] = 1, ["double sword"] = 1,
                     ["war axe"] = 8, ["broad axe"] = 12, ["lajatang"] = 2}

    -- Basic set of egos with weights.
    local egos = {["none"] = 30, ["heavy"] = 15, ["freezing"] = 15,
            ["electrocution"] = 10, ["venom"] = 10, ["protection"] = 10,
            ["vampirism"] = 5, ["holy_wrath"] = 5, ["draining"] = 5,
            ["pain"] = 2, ["distortion"] = 2, ["antimagic"] = 2, ["speed"] = 1}

    local quality = crawl.one_chance_in(3) and "randart" or "good_item"
    local weapon_def = dgn.random_item_def(weapons, egos, quality)
    e.kitem("O = " .. weapon_def)
end

function wrathful_weapon(class, quality)
    local egos
    -- Only the egos Trog gifts, using equal weights for good_item but giving
    -- the more desirable antimagic brand for randarts, which are trying to be
    -- better quality.
    if quality == "good_item" then
        egos = {["antimagic"] = 10, ["heavy"] = 10, ["flaming"] = 10}
    elseif quality == "randart" then
        -- Increased chance for antimagic.
        egos = {["antimagic"] = 25, ["heavy"] = 10, ["flaming"] = 10}
    else
        error("Unknown weapon quality: " .. quality)
    end

    return dgn.monster_weapon(class, egos, quality)
 end

function disto_weapon(class, quality)
    return dgn.monster_weapon(class, "distortion", quality)
end

-- Determine the number of gold piles placed for ebering_ghost_gozag.
-- Mean ranges from 3 to 15 from D:3-D:15 with
-- 12-13 in Orc and 15 elsewhere, and the actual number placed varies by +4/-2.
function setup_gozag_gold(e)
    if dgn.persist.necropolis_difficulty == "orc" then
        depth = 12
    else
        depth = you.absdepth()
    end
    pile_mean = math.min(15, math.floor(5/4 * depth + 3.75))
    pile_count = crawl.random_range(pile_mean - 2, pile_mean + 4)
    e.nsubst("' = " .. pile_count .. ":$ / *:-")
end

-- Set up the chaos dancing weapon for ebering_necropolis_ghost_xom.
function setup_xom_dancing_weapon(e, glyph)
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
    e.kmons(glyph .. " = dancing weapon ; " .. weapon_def)
end

-- Set up equipment for the fancier orc warriors, knights, and warlord in
-- biasface_ghost_orc_armoury.
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


-- Some of the less special but still distinct items from the Xom bazaar.
function xom_bazaar_special(e, glyph)
    local sp = util.random_subset({ "discord", "death's_door", "malign_gateway",
                                    "summon_horrible_things",
                                    "fulsome_fusillade" }, 2)
    local plus = crawl.random_range(4, 8)

    e.kitem(glyph .. " = any talisman / any talisman randart / " ..
                        "orb ego:mayhem randart artprops:Int:" .. plus .. " / " ..
                        "orb ego:wrath randart artprops:Str:" .. plus .. " / " ..
                        "orb ego:guile randart artprops:Dex:" .. plus .. " / " ..
                        "randbook owner:Xom spells:" ..table.concat(sp, "&&"))
end
