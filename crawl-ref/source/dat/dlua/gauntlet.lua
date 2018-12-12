-- Set the arena subvault tier. This must be called by main gauntlet encompass
-- map before the subvault statement.
--
-- @param n The tier, currently either 1 or 2.
function gauntlet_arena_set_tier(n)
    gauntlet_arena_tier = n
end

-- Set a random monster list based on the monsters in tier1_gauntlet_arenas.
-- Use only these so that the potential summons won't get too crazy. This isn't
-- a great system since custom weights in the "mons" entries could throw this
-- off.
function gauntlet_random_mons_setup(e)
    local mon_list = ""
    for _, entry in ipairs(tier1_gauntlet_arenas) do
        for _, key in ipairs({"first", "second", "third"}) do
            if entry[key] ~= nil then
                if mon_list == "" then
                    mon_list = entry[key]["mons"]
                else
                    mon_list = mon_list .. " / " .. entry[key]["mons"]
                end
            end
        end
    end

   e.set_random_mon_list(mon_list)
end

-- Main setup function for gauntlet encompass maps. See comments in the main
-- maps section of gauntlet.des for details.
--
-- @param e            Lua environment.
-- @param entry_glyphs A string of glyphs for entry transporter glyphs in order
--                     of subvault placement.
-- @param exit_glyphs  A string of glyphs for exit transporter landing site
--                     glyphs in order of subvault placement.
function gauntlet_setup(e, entry_glyphs, exit_glyphs)
    for i = 1, entry_glyphs:len() do
        e.lua_marker(entry_glyphs:sub(i,i),
                     transp_loc("gauntlet_arena_entry_" ..  tostring(i)))
        e.lua_marker(exit_glyphs:sub(i,i),
                     transp_dest_loc("gauntlet_arena_exit_" ..  tostring(i)))
    end

    gauntlet_arena_set_tier(1)
    gauntlet_arena_num = nil

    gauntlet_random_mons_setup(e)
end

-- Set up transporter features on glyphs 'P' and 'Q' based on the current
-- subvault number.
-- @param e Lua environment.
function gauntlet_arena_transporter_setup(e)
    if gauntlet_arena_num == nil then
        gauntlet_arena_num = 1
    else
        gauntlet_arena_num = gauntlet_arena_num + 1
    end

    e.lua_marker("P", transp_dest_loc("gauntlet_arena_entry_" ..
                                      tostring(gauntlet_arena_num)))
    e.lua_marker("Q", transp_loc("gauntlet_arena_exit_" ..
                                 tostring(gauntlet_arena_num)))
end

-- Get a random arena entry for an arena subvault based on the arena's tier.
-- @param e Lua environment.
function gauntlet_arena_get_monster_entry(e)
    local gauntlet_arenas
    if gauntlet_arena_tier == 1 then
        gauntlet_arenas = tier1_gauntlet_arenas
    else
        gauntlet_arenas = tier2_gauntlet_arenas
    end

    return util.random_weighted_from("weight", gauntlet_arenas)
end

-- Make a KMONS statement based on a given monster entry and glyph. Roll the
-- number of monsters to place and place them on that glyph.
-- @param e     Lua environment.
-- @param entry A table with keys 'mons', 'min', and 'max' See the comments
--              above the variable tier1_gauntlet_arenas.
-- @param glyph The glyph on which to place the entry. If entry is nil, this
--              glyph will be replaced with floor.
function gauntlet_arena_mons_setup(e, entry, glyph)
    if entry == nil then
        e.subst(glyph .. " = .")
        return
    end

    e.kmons(glyph .. " = " .. entry["mons"])

    local n = entry["min"] + crawl.random2(entry["max"] - entry["min"] + 1)
    if n < 1 then
        e.subst(glyph .. " = .")
    else
        e.nsubst(glyph .. " = " .. tostring(n) .. "=" .. glyph .. " / .")
    end
end

-- Set up item definitions for arena subvaults.
-- @param e          Lua environment.
-- @param other_loot If non-nil, place this items as a guaranteed loot item.
function gauntlet_arena_item_setup(e, other_loot)
    -- If an entry defines loot, one of that item will always place, otherwise
    -- 50% chance of good scroll or potion and 50% chance of star_item.
    local d_first_nsubst = "d*"
    if other_loot then
        e.item(other_loot)
        d_first_nsubst = "d"
    else
        e.item(dgn.loot_scrolls .. " / " .. dgn.loot_potions)
    end

    -- For tier 1 arenas, we place one more item that's either 2/3 chance
    -- superb item or star_item and 1/3 chance for good_item aux or jewellery.
    if gauntlet_arena_tier == 1 then
        e.item(dgn.good_aux_armour)
        e.item("any jewellery good_item")
        e.nsubst("d = " .. d_first_nsubst .. " / ef|*|* / .")
    -- For tier 2
    else
        if crawl.one_chance_in(3) then
            e.item(dgn.randart_aux_armour)
            e.item("any jewellery randart")
        else
            e.item(dgn.good_aux_armour)
            e.item("any jewellery good_item")
        end
        e.nsubst("d = " .. d_first_nsubst .. " / ef / |* / .")

-- Arena subvault terrain setup. Handles randomization of rock, liquids, trap
-- placement, and plants.
-- subvault.
-- @param e              Lua environment.
-- @param rock_unchanged If true, don't randomize 'x' glyphs to other wall
--                       types. Defaults to false.
-- @param liquid         Either nil or one of the strings "lava" or "water". If
--                       nil, 'l' is unchanged and 'w' has a chance to become
--                       lava. If "lava", all 'w' glyphs becomes lava. If
--                       "water", all 'l' glyphs become deep water.
-- @param plant          Either nil or one of the strings "demonic" or
--                       "withered". If nil, 'p' becomes a mix of mostly
--                       plants with some fungus. If "withered" or "demonic",
--                       'p' becomes withered or demonic plants, respectively.
function gauntlet_arena_terrain_setup(e, rock_unchanged, liquid, plant)
    if not rock_unchanged then
        local glyphs = "xxc"
        if gauntlet_arena_tier == 2 then
            glyphs = "cccvvb"
        end
        e.subst("x : " .. glyphs)
    end

    if not liquid then liquid = "default" end
    if liquid == "lava" then
        e.subst("w = l")
    elseif liquid == "water" then
        e.subst("l = w")
    elseif liquid == "default" then
        e.subst("w : wwwl")
    else
        error("Unknown liquid type: " .. liquid)
    end

    if not plant then plant = "default" end
    if plant == "demonic" then
        e.kmons("p = demonic plant")
    elseif plant == "withered" then
        e.kmons("p = withered plant")
    elseif plant == "default" then
        e.kmons("p = plant w:30 / fungus")
    else
        error("Unknown plant type: " .. plant)
    end
end

-- Arena subvault main setup. See the comments in the arena subvault section of
-- gauntlet.des for details.
-- @param e              Lua environment.
-- @param rock_unchanged If true, don't randomize 'x' glyphs to other wall
--                       types. Defaults to false.
function gauntlet_arena_setup(e, rock_unchanged)
    gauntlet_arena_transporter_setup(e)

    local entry = gauntlet_arena_get_monster_entry(e)

    gauntlet_arena_item_setup(e, entry["loot"])

    gauntlet_arena_mons_setup(e, entry["first"], "1")
    gauntlet_arena_mons_setup(e, entry["second"], "2")
    gauntlet_arena_mons_setup(e, entry["third"], "3")

    gauntlet_arena_terrain_setup(e, rock_unchanged, entry["liquid"],
                                 entry["plant"])
end

-- Arena monster sets used for the first vault choice. The monster entries are
-- in the keys 'first', 'second', and 'third' giving the monsters to place on
-- the glyphs '1', '2', and '3', respectively. See the comments in arena
-- subvault section of gauntlet.des for more details.
--
-- Each monster entry must contain a "mons" key with the definition, and "min"
-- and "max" keys. The number of monsters that place for that glyph will then
-- be random uniform on [min, max].
--
-- Other supported keys:
-- "weight" : Choice weight for the arena set, defaults to 10.
-- "liquid" : Either "lava" or "water" will force the arena vault to use
--            only that liquid wherever it would place 'w' or 'l'.
-- "plant"  : Either "demonic" or "withered" will have the arena use that type
--            of plant whenever it would place a plant.
-- "loot"   : Should contain a valid item definition for a custom loot item.
--            Exactly one of this item will always place in the arena.
tier1_gauntlet_arenas = {
  {
    first  = {mons = "spiny frog simulacrum / wyvern simulacrum " ..
                      "/ hornet simulacrum", min = 1, max = 1},
    second = {mons = "simulacrum place:D:12", min = 2, max = 4},
    liquid = "water",
    plant  = "withered",
  },
  {
    second = {mons = "raiju", min = 1, max = 1},
    third  = {mons = "steam dragon", min = 1, max = 2},
    liquid = "water",
  },
  {
    second = {mons = "slime creature", min = 2, max = 4},
    third  = {mons = "jelly", min = 1, max = 3},
    liquid = "water",
    plant  = "demonic",
  },
  {
    first  = {mons = "death ooze", min = 1, max = 1},
    plant  = "withered",
  },
  {
    first  = {mons = "sixfirhy", min = 1, max = 1},
    second = {mons = "ufetubus", min = 2, max = 4},
    liquid = "water",
    plant  = "demonic",
  },
  {
    first  = {mons = "ice devil", min = 1, max = 1},
    second = {mons = "white imp", min = 2, max = 4},
    liquid = "water",
    plant  = "demonic",
  },
  {
    first  = {mons = "soul eater", min = 1, max = 1},
    second = {mons = "shadow imp", min = 2, max = 4},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "ynoxinul", min = 1, max = 1},
    second = {mons = "rust devil", min = 0, max = 1},
    third  = {mons = "iron imp", min = 2, max = 4},
    plant  = "demonic",
  },
  {
    first  = {mons = "smoke demon", min = 1, max = 1},
    second = {mons = "red devil", min = 0, max = 1},
    third  = {mons = "crimson imp", min = 2, max = 4},
    liquid = "lava",
    plant  = "demonic",
  },
  {
    first  = {mons = "neqoxec", min = 1, max = 1},
    second = {mons = "chaos spawn", min = 1, max = 3},
    plant  = "demonic",
  },
  {
    second = {mons = "shadow", min = 2, max = 4},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "catoblepas", min = 1, max = 1},
    second = {mons = "gargoyle", min = 1, max = 2},
  },
  {
    first  = {mons = "torpor snail", min = 1, max = 1},
    second = {mons = "yak band / wolf band / elephant band", min = 1,
               max = 1},
  },
  {
    first  = {mons = "fire crab", min = 1, max = 1},
    second = {mons = "fire bat", min = 2, max = 4},
    liquid = "lava",
  },
  {
    second = {mons = "shapeshifter", min = 1, max = 3},
  },
  {
    first  = {mons = "flying skull band", min = 1, max = 1},
    plant  = "withered",
  },
  {
    first  = {mons = "queen ant", min = 1, max = 1},
    second = {mons = "soldier ant", min = 3, max = 6},
  },
  {
    second = {mons = "hornet", min = 2, max = 3},
  },
  {
    first  = {mons = "queen bee", min = 1, max = 1},
    second = {mons = "killer bee band", min = 1, max = 1},
  },
  {
    first  = {mons = "moth of wrath", min = 1, max = 1},
    second = {mons = "wolf band", min = 1, max = 1},
  },
  {
    first  = {mons = "worldbinder", min = 1, max = 1},
    second = {mons = "boggart", min = 1, max = 3},
    plant  = "demonic",
  },
  {
    first  = {mons = "death scarab", min = 1, max = 1},
    second = {mons = "spectre place:Lair:6", min = 1, max = 1},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "shining eye", min = 1, max = 1},
    second = {mons = "floating eye / golden eye", min = 1, max = 2},
    loot   = "potion of mutation ident:type",
    plant  = "demonic",
    weight = 5,
  },
  {
    first  = {mons = "eye of devastation", min = 1, max = 1},
    second = {mons = "floating eye / golden eye", min = 1, max = 2},
    weight = 5,
  },
  {
    first  = {mons = "tengu warrior", min = 1, max = 1},
    second = {mons = "tengu conjurer", min = 1, max = 2},
  },
  {
    first  = {mons = "wizard", min = 1, max = 1},
    second = {mons = "white imp / shadow imp", min = 1, max = 3},
  },
  {
    second = {mons = "large abomination", min = 0, max = 2},
    third  = {mons = "small abomination", min = 3, max = 6},
    plant  = "demonic",
  },
  {
    first  = {mons = "ice statue", min = 1, max = 1},
    second = {mons = "rime drake", min = 1, max = 2},
    liquid = "water",
  },
  {
    first  = {mons = "oklob plant", min = 1, max = 1},
    second = {mons = "acid dragon", min = 1, max = 2},
  },
  {
    first  = {mons = "necromancer", min = 1, max = 1},
    second = {mons = "wolf band", min = 1, max = 1},
    plant  = "withered",
  },
  {
    second = {mons = "swamp worm", min = 2, max = 3},
  },
}

-- Monster sets used for the first vault choice, same structure as for
-- tier1_gauntlet_arenas.
tier2_gauntlet_arenas = {
  {
    first  = {mons = "hydra simulacrum / harpy simulacrum " ..
                      "/ ice dragon simulacrum", min = 1, max = 1},
    second = {mons = "simulacrum place:D:15", min = 2, max = 4},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "dire elephant", min = 1, max = 1},
    second = {mons = "elephant", min = 1, max = 3},
  },
  {
    first  = {mons = "catoblepas", min = 1, max = 1},
    second = {mons = "molten gargoyle", min = 1, max = 2},
    third  = {mons = "gargoyle", min = 1, max = 2},
    liquid = "lava",
  },
  {
    second = {mons = "shadow", min = 3, max = 6},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "fire crab", min = 1, max = 1},
    second = {mons = "hell hound", min = 1, max = 3},
    third  = {mons = "fire bat", min = 3, max = 6},
    liquid = "lava",
  },
  {
    first  = {mons = "efreet", min = 1, max = 1},
    second = {mons = "hell hound", min = 1, max = 3},
    third  = {mons = "fire elemental", min = 1, max = 3},
    liquid = "lava",
    plant  = "demonic",
  },
  {
    first  = {mons = "hell hog", min = 1, max = 1},
    second = {mons = "hell hound", min = 1, max = 3},
    third  = {mons = "fire bat", min = 2, max = 4},
    liquid = "lava",
    plant  = "demonic",
  },
  {
    second = {mons = "death ooze", min = 1, max = 1},
    third  = {mons = "jelly w:5 / slime creature", min = 2, max = 4},
    liquid = "water",
  },
  {
    second = {mons = "sixfirhy", min = 2, max = 4},
    liquid = "water",
    plant  = "demonic",
  },
  {
    first  = {mons = "hell beast", min = 1, max = 1},
    second = {mons = "hell hound", min = 0, max = 2},
    liquid = "lava",
    plant  = "demonic",
  },
  {
    first  = {mons = "green death", min = 1, max = 1},
    second = {mons = "orange demon", min = 0, max = 2},
    plant  = "demonic",
  },
  {
    first  = {mons = "hellion", min = 1, max = 1},
    second = {mons = "sun demon / smoke demon / red devil", min = 0, max = 1},
    third  = {mons = "hell hound", min = 0, max = 2},
    liquid = "lava",
    plant  = "demonic",
    weight = 5
  },
  {
    first  = {mons = "balrug", min = 1, max = 1},
    third  = {mons = "sun demon", min = 0, max = 1},
    third  = {mons = "hell hound", min = 0, max = 2},
    liquid = "lava",
    plant  = "demonic",
    weight = 5
  },
  {
    first  = {mons = "cacodemon", min = 1, max = 1},
    second = {mons = "neqoxec", min = 0, max = 3},
    loot   = "potion of mutation ident:type",
    plant  = "demonic",
    weight = 5
  },
  {
    first  = {mons = "blizzard demon", min = 1, max = 1},
    second = {mons = "white imp / ice devil", min = 0, max = 3},
    liquid = "water",
  },
  {
    first  = {mons = "reaper", min = 1, max = 1},
    second = {mons = "hellwing / shadow imp", min = 1, max = 3},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "glowing shapeshifter", min = 1, max = 1},
    second = {mons = "shapeshifter", min = 2, max = 3},
  },
  {
    first  = {mons = "apocalypse crab", min = 1, max = 1},
    plant  = "demonic",
  },
  {
    second = {mons = "flying skull band", min = 2, max = 3},
    plant  = "withered",
  },
  {
    first  = {mons = "death drake", min = 1, max = 1},
    second = {mons = "zombie place:D:15 / spectre place:D:15", min = 2,
              max = 4},
    liquid = "water",
    plant  = "withered"
  },
  {
    first  = {mons = "ghost crab", min = 1, max = 1},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "meliai band", min = 1, max = 1},
  },
  {
    first  = {mons = "torpor snail", min = 1, max = 1},
    second = {mons = "death yak band / hydra", min = 1, max = 1},
  },
  {
    first  = {mons = "entropy weaver", min = 1, max = 1},
    second = {mons = "wolf spider", min = 0, max = 1},
    third  = {mons = "redback / jumping spider", min = 1, max = 2},
    liquid = "water",
  },
  {
    first  = {mons = "moth of wrath", min = 1, max = 1},
    second = {mons = "wolf spider / nothing", min = 0, max = 1},
    third  = {mons = "redback", min = 2, max = 4},
    liquid = "water",
  },
  {
    second = {mons = "thrashing horror", min = 1, max = 2},
    third  = {mons = "small abomination", min = 2, max = 4},
    plant  = "demonic",
  },
  {
    first  = {mons = "great orb of eyes", min = 1, max = 1},
    second = {mons = "ugly thing", min = 1, max = 2},
    plant  = "demonic",
  },
  {
    second = {mons = "glowing orange brain", min = 1, max = 2},
    third  = {mons = "ugly thing", min = 1, max = 2},
    plant  = "demonic",
  },
  {
    second = {mons = "large abomination", min = 2, max = 3},
    third  = {mons = "small abomination", min = 3, max = 6},
    plant  = "demonic",
  },
  {
    first  = {mons = "wretched star", min = 1, max = 1},
    second = {mons = "neqoxec / small abomination", min = 1, max = 3},
    loot   = "potion of mutation ident:type",
    plant  = "demonic",
  },
  {
    second = {mons = "wizard", min = 2, max = 2},
    third  = {mons = "white imp / shadow imp", min = 3, max = 6},
  },
  {
    first  = {mons = "necromancer", min = 1, max = 1},
    second = {mons = "death yak band", min = 1, max = 1},
    liquid = "water",
    plant  = "withered",
  },
  {
    second = {mons = "death scarab", min = 2, max = 2},
    third  = {mons = "spectre place:Lair:6", min = 2, max = 4},
    liquid = "water",
    plant  = "withered",
  },
  {
    first  = {mons = "orange crystal statue", min = 1, max = 1},
  },
  {
    first  = {mons = "obsidian statue", min = 1, max = 1},
    liquid = "water",
    plant  = "withered",
  },
  {
    -- Guarantee that one places in the central location.
    first  = {mons = "lightning spire", min = 1, max = 1},
    second = {mons = "lightning spire", min = 0, max = 1},
    third  = {mons = "raiju", min = 1, max = 2},
  },
  {
    first  = {mons = "tyrant leech", min = 1, max = 1},
    second = {mons = "swamp worm", min = 2, max = 4},
  },
}

function gauntlet_exit_loot(e)
    local gauntlet_loot = "superb_item w:49 / any armour w:7 " ..
                          "/ any wand w:14 / any scroll"

    local num_items = 7 + crawl.random2avg(10, 2)
    local item_def = ""
    for i = 1, num_items do
        if i > 1 then
            item_def = item_def .. ", "
        end

        item_def = item_def ..
                   (crawl.one_chance_in(3) and "star_item" or gauntlet_loot)
    end

    e.kitem("< = " .. item_def)
end
