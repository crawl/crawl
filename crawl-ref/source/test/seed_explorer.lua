-- tools for seeing what's in a seed. Currently packaged as a test that prints
-- out a few levels from some early seeds in detail, but fairly configurable.
-- TODO: maybe move some of the core code out into dat/dlua?

you.enter_wizard_mode() -- necessary so that magic mapping behaves correctly.
                        -- this is implied by you.save = false, so it shouldn't
                        -- affect other tests in actual test mode...

-- to use a >32bit seed, you will need to use a string here. If you use a
-- string, `fixed_seed` is maxed at 1.
local starting_seed = 1   -- fixed seed to start with, number or string
local fixed_seeds = 5     -- how many fixed seeds to run? Can be 0.
local rand_seeds = 0      -- how many random seeds to run

-- matches pregeneration order
local generation_order = {
                "D:1",
                "D:2", "D:3", "D:4", "D:5", "D:6", "D:7", "D:8", "D:9",
                "D:10", "D:11", "D:12", "D:13", "D:14", "D:15",
                "Temple",
                "Lair:1", "Lair:2", "Lair:3", "Lair:4", "Lair:5", "Lair:6",
                "Orc:1", "Orc:2",
                "Spider:1", "Spider:2", "Spider:3", "Spider:4",
                "Snake:1", "Snake:2", "Snake:3", "Snake:4",
                "Shoals:1", "Shoals:2", "Shoals:3", "Shoals:4",
                "Swamp:1", "Swamp:2", "Swamp:3", "Swamp:4",
                "Vaults:1", "Vaults:2", "Vaults:3", "Vaults:4", "Vaults:5",
                "Crypt:1", "Crypt:2", "Crypt:3",
                "Depths:1", "Depths:2", "Depths:3", "Depths:4", "Depths:5",
                "Hell",
                "Elf:1", "Elf:2", "Elf:3",
                "Zot:1", "Zot:2", "Zot:3", "Zot:4", "Zot:5",
                "Slime:1", "Slime:2", "Slime:3", "Slime:4", "Slime:5",
                "Tomb:1", "Tomb:2", "Tomb:3",
                "Tar:1", "Tar:2", "Tar:3", "Tar:4", "Tar:5", "Tar:6", "Tar:7",
                "Coc:1", "Coc:2", "Coc:3", "Coc:4", "Coc:5", "Coc:6", "Coc:7",
                "Dis:1", "Dis:2", "Dis:3", "Dis:4", "Dis:5", "Dis:6", "Dis:7",
            }

-- this variable determines how deep in the generation order to go
--local max_depth = #generation_order
local max_depth = 5

------------------------------
-- some generic list/string manipulation code

function collapse_dups(l)
    local result = {}
    table.sort(l)
    local cur = ""
    local count = 0
    for i, name in ipairs(l) do
        if name == cur then
            count = count + 1
        else
            if (cur ~= "") then
                result[#result + 1] = { cur, count }
            end
            cur = name
            count = 1
        end
    end
    if (cur ~= "") then
        result[#result + 1] = { cur, count }
    end
    for i, name in ipairs(result) do
        if result[i][2] > 1 then
            result[i] = result[i][1] .. " x" .. result[i][2]
        else
            result[i] = result[i][1]
        end
    end
    return result
end

function fancy_join(l, indent, width, sep, initial_indent)
    -- TODO: reflow around spaces?
    if #l == 0 then
        return ""
    end
    local lines = {}
    local cur_line = string.rep(" ", indent)
    for i, s in ipairs(l) do
        local full_s = s
        if (i ~= #l) then
            full_s = full_s .. sep
        end
        if #cur_line + #full_s > width and i ~= 1 then
            lines[#lines + 1] = cur_line
            cur_line = string.rep(" ", indent)
        end
        cur_line = cur_line .. full_s
    end
    lines[#lines + 1] = cur_line
    if not initial_indent and #lines > 0 and indent > 0 then
        lines[1] = string.sub(lines[1], indent + 1, #lines[1])
    end
    return table.concat(lines, "\n")
end

------------------------------
-- code for looking at items

function arts_only(item)
    return item.artefact
end

function item_ignore_boring(item)
    -- TODO:
    --  show gold totals?
    --  missiles - early on?
    --  unenchanted weapons/armour - early on?
    if item.is_useless then
        return false
    elseif item.base_type == "gold"
            or item.base_type == "missile"
            or item.base_type == "food" then
        return false
    elseif (item.base_type == "weapon" or item.base_type == "armour")
            and item.pluses() <= 0 and not item.branded then
        return false
    end
    return true
end

local item_notable = item_ignore_boring

function catalog_items()
    wiz.identify_all_items()
    local gxm, gym = dgn.max_bounds()
    local notable = {}
    for p in iter.rect_iterator(dgn.point(1,1), dgn.point(gxm-2, gym-2)) do
        local stack = dgn.items_at(p.x, p.y)
        if #stack > 0 then
            for i, item in ipairs(stack) do
                if item_notable(item) then
                    notable[#notable + 1] = item.name()
                end
            end
        end
        -- TODO: shops
    end
    return notable
end

------------------------------
-- code for looking at features

local hell_branches = util.set{ "Geh", "Coc", "Dis", "Tar" }

function in_hell()
    return hell_branches[you.branch()]
end

function feat_interesting(feat_name)
    -- most features are pretty boring...
    if string.find(feat_name, "altar_") == 1 then
        return true
    elseif string.find(feat_name, "enter_") == 1 then -- could be more selective
        if in_hell() then
            return feat_name ~= "enter_hell"
        else
            return true
        end
    elseif feat_name == "transporter" or string.find(feat_name, "runed_") then
        return true
    end
    return false
end

function catalog_features()
    -- TODO: this could be a lot more efficient in skipping things like walls
    wiz.map_level() -- abyss will break this call...
    local gxm, gym = dgn.max_bounds()
    local notable = {}
    for p in iter.rect_iterator(dgn.point(1,1), dgn.point(gxm-2, gym-2)) do
        local feat = dgn.grid(p.x, p.y)
        --crawl.stderr(dgn.feature_desc(feat))
        if feat_interesting(dgn.feature_name(feat)) then
            notable[#notable + 1] = dgn.feature_desc_at(p.x, p.y, "A")
        end
    end
    return notable
end

------------------------------
-- code for looking at monsters

-- fairly subjective, some of these should probably be relative to depth
local always_interesting_mons = util.set{
    "ancient lich",
    "orb of fire",
    "greater mummy",
    "Hell Sentinel",
    "Ice Fiend",
    "Brimstone Fiend",
    "Tzitzimitl",
    "shard shrike",
    "caustic shrike",
    "iron giant",
    "juggernaut",
    "Killer Klown",
    "Orb Guardian",
    "curse toe",
}

function mons_ignore_boring(mons)
    return (mons.unique
            or always_interesting_mons[mons.name]
            or mons.type_name == "player ghost"
            or mons.type_name == "pandemonium lord")
end

local mons_notable = mons_ignore_boring
--local mons_notable = function (m) return false end

function mons_always_native(m, mi)
    return (mi:is_unique()
            or m.type_name == "player ghost"
            or mi:is_firewood())
end

function rare_ood(mi)
    local depth = mi:avg_local_depth()
    -- TODO: really, probability can't be interpreted without some sense of what
    -- the overall distribution for the branch is, so this is fairly heuristic
    local prob = mi:avg_local_prob()
    local ood_threshold = math.max(2, dgn.br_depth() / 3)
    return depth > you.depth() + ood_threshold and prob < 2
end

function feat_in_set(s)
    return function (f) if s[f] then return f else return nil end end
end

--mons_feat_filter = feat_in_set(util.set({ "OOD" }))
mons_feat_filter = nil

function describe_mons(mons)
    local mi = mons.get_info()
    -- TODO: weird distribution of labor between mons and moninfo, can this be
    -- cleaned up?
    -- TODO: does it make sense to use the same item notability function here?
    local feats = util.map(function (i) return i.name() end,
                    util.filter(item_notable, mons.get_inventory()))
    local force_notable = false
    if mons_feat_filter then
        feats = util.map(mons_feat_filter, feats) -- TODO don't brute force this
    end
    if mons.type_name == "dancing weapon" and #feats > 0 then
        -- don't repeat the dancing weapon's item, but if the item_notable check
        -- has deemed the item notable, make sure to show it.
        feats = { }
        force_notable = true
    end

    -- may or may not be useful -- OOD is an indicator of builder choices, not
    -- intrinsic monster quality, and it is possible for monsters to be chosen
    -- as OOD that are within range, e.g. you can sometimes get the same monster
    -- generating as OOD and not OOD.
    -- TODO: maybe worth rethinking what counts as OOD? might be something like,
    -- average spawn depth for monster in branch is greater than depth + 5.
    -- if mons.has_prop("mons_is_ood") then
    --     feats[#feats + 1] = "builder OOD"
    -- end
    if rare_ood(mi) then
        feats[#feats + 1] = "OOD"
    end
    if not mons.in_local_population and not mons_always_native(mons, mi) then
        -- this is a different sense of "native" than used internally to crawl,
        -- in that it includes anything that would normally generate in a
        -- branch. The internal sense is just supposed to be about which
        -- monsters "live" in a particular branch, which is covered as well.
        feats[#feats + 1] = "non-native"
    end
    if mons_feat_filter then
        feats = util.map(mons_feat_filter, feats)
    end
    local feat_string = ""
    if #feats > 0 then
        feat_string = " (" .. table.concat(feats, ", ") .. ")"
    end

    local name_string = "bug"

    if mons.type_name == "player ghost" then
        name_string = mons.type_name
    else
        name_string = mons.name
    end
    if (force_notable or mons_notable(mons) or #feats > 0) then
        return name_string .. feat_string
    else
        return nil
    end
end

function get_all_monsters()
    local gxm, gym = dgn.max_bounds()
    local mons_list = {}
    for p in iter.rect_iterator(dgn.point(1,1), dgn.point(gxm-2, gym-2)) do
        local mons = dgn.mons_at(p.x, p.y)
        if mons then
            mons_list[#mons_list + 1] = mons
        end
    end
    return mons_list
end

function catalog_monsters()
    wiz.identify_all_items()
    return util.map(describe_mons, get_all_monsters())
end

function catalog_vaults()
    -- TODO: just pass the list directly to lua...
    local vaults = debug.vault_names()
    first_split = crawl.split(vaults, " and ")
    if #first_split < 2 then
        return first_split
    end
    second_split = crawl.split(first_split[1], ", ")
    second_split[#second_split + 1] = first_split[2]
    return second_split
end

function make_highlight(h, key, name, to_show, hide_empty)
    if not h[key] or not to_show[key] then
        return ""
    end
    local l = h[key]
    if key ~= "vaults" then
        l = collapse_dups(l)
    end
    s = fancy_join(l, #name, 80, ", ")
    if (#s == 0 and hide_empty) then
        return ""
    end
    return name .. s
end

function catalog_current_place(lvl, to_show, hide_empty)
    -- TODO: features (mainly just altars), shops
    highlights = {}
    highlights["vaults"] = catalog_vaults()
    highlights["items"] = catalog_items()
    highlights["monsters"] = catalog_monsters()
    highlights["features"] = catalog_features()
    h_l = { }
    h_l[#h_l+1] = make_highlight(highlights, "vaults",   "   Vaults: ", to_show, hide_empty)
    h_l[#h_l+1] = make_highlight(highlights, "monsters", " Monsters: ", to_show, hide_empty)
    h_l[#h_l+1] = make_highlight(highlights, "items",    "    Items: ", to_show, hide_empty)
    h_l[#h_l+1] = make_highlight(highlights, "features", " Features: ", to_show, hide_empty)
    h_l = util.filter(function (a) return #a > 0 end, h_l)
    if #h_l > 0 or not hide_empty then
        crawl.stderr(lvl .. " highlights:")
        for _, h in ipairs(h_l) do
            crawl.stderr(h)
        end
    end
    return highlights
end

-- a bit redundant with mapstat?
function catalog_dungeon(level_order, to_show)
    local result = {}
    dgn.reset_level()
    debug.flush_map_memory()
    debug.dungeon_setup()
    for i,lvl in ipairs(level_order) do
        if i > max_depth then break end
        if (dgn.br_exists(string.match(lvl, "[^:]+"))) then
            debug.goto_place(lvl)
            debug.generate_level()
            result[lvl] = catalog_current_place(lvl, to_show, true)
        end
    end
    return result
end

function seed_items(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Item catalog for seed " .. seed .. ":")
    local run1 = catalog_dungeon(generation_order, util.set{ "items" })
end

function seed_arts(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Artefact catalog for seed " .. seed .. ":")
    item_notable = arts_only
    local run1 = catalog_dungeon(generation_order, util.set{ "items" })
    item_notable = ignore_boring
end

function seed_mons(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Monster catalog for seed " .. seed .. ":")
    local run1 = catalog_dungeon(generation_order, util.set{ "monsters" })
end

function seed_feats(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Dungeon feature catalog for seed " .. seed .. ":")
    local run1 = catalog_dungeon(generation_order, util.set{ "features" })
end

function seed_full_catalog(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Catalog for seed " .. seed .. ":")
    local run1 = catalog_dungeon(generation_order, util.set{ "vaults", "items", "features", "monsters" })
end

function catalog_seeds(seeds, cat_fun)
    for _, i in ipairs(seeds) do
        if _ > 1 then crawl.stderr("") end
        cat_fun(i)
    end
end

if fixed_seeds > 0 then
    local seed_seq = { starting_seed }

    if (type(starting_seed) ~= "string") then
        for i=starting_seed + 1, starting_seed + fixed_seeds - 1 do
            seed_seq[#seed_seq + 1] = i
        end
    end
    catalog_seeds(seed_seq, seed_full_catalog)
end

if rand_seeds > 0 then
    local rand_seq = { }
    math.randomseed(crawl.millis())
    for i=1,rand_seeds do
        -- intentional use of non-crawl random(). random doesn't seem to accept
        -- anything bigger than 32 bits for the range.
        rand_seed = math.random(0x7FFFFFFF)
        rand_seq[#rand_seq + 1] = rand_seed
    end
    crawl.stderr("Exploring " .. rand_seeds .. " random seed(s).")
    catalog_seeds(rand_seq, seed_full_catalog)
end
