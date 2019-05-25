-- tools for seeing what's in a seed. Currently packaged as a test that prints
-- out a few levels from some early seeds in detail, but fairly configurable.
-- TODO: maybe move some of the core code out into dat/dlua?

local starting_seed = 1   -- fixed seed to start with
local fixed_seeds = 5     -- how many fixed seeds to run?
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

function describe_mons(mons)
    -- TODO: does it make sense to use the same item notability function here?
    local feats = util.map(function (i) return i.name() end,
                    util.filter(item_notable, mons.get_inventory()))
    -- may or may not be useful -- OOD is an indicator of builder choices, not
    -- intrinsic monster quality, and it is possible for monsters to be chosen
    -- as OOD that are within range, e.g. you can sometimes get the same monster
    -- generating as OOD and not OOD.
    -- TODO: maybe worth rethinking what counts as OOD? might be something like,
    -- average spawn depth for monster in branch is greater than depth + 5.
    if mons.has_prop("mons_is_ood") then
        feats[#feats + 1] = "OOD"
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
    if (mons_notable(mons) or #feats > 0) then
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
    h_l = { }
    h_l[#h_l+1] = make_highlight(highlights, "vaults",   "   Vaults: ", to_show, hide_empty)
    h_l[#h_l+1] = make_highlight(highlights, "monsters", " Monsters: ", to_show, hide_empty)
    h_l[#h_l+1] = make_highlight(highlights, "items",    "    Items: ", to_show, hide_empty)
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

function seed_full_catalog(seed)
    seed_used = debug.reset_rng(seed)
    crawl.stderr("Catalog for seed " .. seed .. ":")
    local run1 = catalog_dungeon(generation_order, util.set{ "vaults", "items", "monsters" })
end

function catalog_seeds(seeds, cat_fun)
    for _, i in ipairs(seeds) do
        cat_fun(i)
    end
end

local seed_seq = { starting_seed }

if (type(starting_seed) ~= "string") then
    for i=starting_seed + 1, starting_seed + fixed_seeds - 1 do
        seed_seq[#seed_seq + 1] = i
    end
end

catalog_seeds(seed_seq, seed_full_catalog)

local rand_seq = { }
math.randomseed(crawl.millis())
for i=1,rand_seeds do
    -- intentional use of non-crawl random(). random doesn't seem to accept
    -- anything bigger than 32 bits for the range.
    rand_seed = math.random(0x7FFFFFFF)
    rand_seq[#rand_seq + 1] = rand_seed
end

if rand_seeds > 0 then
    crawl.stderr("Exploring " .. rand_seeds .. " random seed(s).")
    catalog_seeds(rand_seq, seed_items)
end
