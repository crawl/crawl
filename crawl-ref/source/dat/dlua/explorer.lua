-- tools for seeing what's in a seed. For example use cases, see both
-- crawl-ref/source/scripts/seed_explorer.lua, and
-- crawl-ref/source/tests/seed_explorer.lua.
-- TODO: LDoc this

util.namespace('explorer')

-- matches pregeneration order
explorer.generation_order = {
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
                "Geh:1", "Geh:2", "Geh:3", "Geh:4", "Geh:5", "Geh:6", "Geh:7",
            }
-- generation order continues: pan, zig. However, these are really only in the
-- official generation order so that entering them forces the rest of the
-- dungeon to generate first, so we ignore them here.

explorer.portal_order = {
    "Sewer",
    "Ossuary",
    "IceCv",
    "Volcano",
    "Bailey",
    "Gauntlet",
    "Bazaar",
    -- Trove is not pregenerated, so should be ignored here
    "WizLab",
    "Desolation"
}


function explorer.level_to_gendepth(lvl)
    -- TODO could parse l, handle things like Hell:1
    for i, l in ipairs(explorer.generation_order) do
        if lvl:lower() == l:lower() then
            return i
        end
    end
    for i, l in ipairs(explorer.portal_order) do
        if lvl:lower() == l:lower() then
            return -i -- fairly hacky
        end
    end
    return nil
end

function explorer.branch_to_gendepth(b)
    if dgn.br_exists(b) then
        local depth = dgn.br_depth(b)
        if depth == 1 then
            return explorer.level_to_gendepth(b)
        else
            return explorer.level_to_gendepth(b .. ":" .. depth)
        end
    end
    return nil
end

function explorer.to_gendepth(depth)
    if type(depth) == "number" then return depth end
    if depth == "all" then return #explorer.generation_order end
    local num = string.match(depth, "^%d+$")
    if num ~= nil then return tonumber(depth) end
    local result = explorer.level_to_gendepth(depth)
    if result == nil then
        result = explorer.branch_to_gendepth(depth)
    end
    return result
end

-- a useful depth preset
explorer.zot_depth = explorer.to_gendepth("Zot")
assert(explorer.zot_depth ~= 0)

-- TODO: generalize, allow changing?
local out = function(s) if not explorer.quiet then crawl.stderr(s) end end

------------------------------
-- some generic list/string manipulation code

function explorer.collapse_dups(l)
    local result = {}
    util.sort(l)
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

function explorer.fancy_join(l, indent, width, sep, initial_indent)
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

function explorer.arts_only(item)
    return item.artefact
end

function explorer.item_ignore_boring(item)
    -- TODO:
    --  show gold totals?
    --  missiles - early on?
    --  unenchanted weapons/armour - early on?
    if item.is_useless then
        return false
    elseif item.base_type == "gold"
            or item.base_type == "missile" then
        return false
    elseif (item.base_type == "weapon" or item.base_type == "armour")
            and item.pluses() <= 0 and not item.branded then
        return false
    end
    return true
end

function explorer.catalog_items_setup()
    wiz.identify_all_items()
end

function explorer.catalog_items(pos, notable)
    local stack = dgn.items_at(pos.x, pos.y)
    if #stack > 0 then
        for i, item in ipairs(stack) do
            if explorer.item_notable(item) then
                notable[#notable + 1] = item.name()
            end
        end
    end
    stack = dgn.shop_inventory_at(pos.x, pos.y)
    if stack ~= nil and #stack > 0 then
        for i, item in ipairs(stack) do
            if explorer.item_notable(item[1]) then
                notable[#notable + 1] = item[1].name() .. " (cost: " .. item[2] ..")"
            end
        end
    end
end

------------------------------
-- code for looking at features

explorer.hell_branches = util.set{ "Geh", "Coc", "Dis", "Tar" }

function explorer.in_hell()
    return explorer.hell_branches[you.branch()]
end

function explorer.feat_interesting(feat_name)
    -- most features are pretty boring...
    if string.find(feat_name, "altar_") == 1 then
        return true
    elseif string.find(feat_name, "enter_") == 1 then -- could be more selective
        if explorer.in_hell() then
            return feat_name ~= "enter_hell"
        else
            return true
        end
    elseif feat_name == "transporter" or string.find(feat_name, "runed_") then
        return true
    end
    return false
end

function explorer.catalog_features_setup()
    you.enter_wizard_mode() -- necessary so that magic mapping behaves
                            -- correctly. this is implied by you.save = false,
                            -- so it shouldn't affect other tests in actual
                            -- test mode...
    wiz.map_level() -- abyss will break this call...
end

function explorer.catalog_features(pos, notable)
    local feat = dgn.grid(pos.x, pos.y)
    if explorer.feat_notable(dgn.feature_name(feat)) then
        notable[#notable + 1] = dgn.feature_desc_at(pos.x, pos.y, "A")
    end
end

------------------------------
-- code for looking at monsters

function explorer.mons_ignore_boring(mons)
    return (mons.unique
            or explorer.always_interesting_mons[mons.name]
            or mons.type_name == "player ghost"
            or mons.type_name == "pandemonium lord")
end

function explorer.mons_always_native(m, mi)
    return (mi:is_unique()
            or m.type_name == "player ghost"
            or mi:is_firewood())
end

function explorer.rare_ood(mi)
    local depth = mi:avg_local_depth()
    -- TODO: really, probability can't be interpreted without some sense of what
    -- the overall distribution for the branch is, so this is fairly heuristic
    local prob = mi:avg_local_prob()
    local ood_threshold = math.max(2, dgn.br_depth() / 3)
    return depth > you.depth() + ood_threshold and prob < 2
end

function explorer.feat_in_set(s)
    return function (f) if s[f] then return f else return nil end end
end

function explorer.describe_mons(mons)
    local mi = mons.get_info()
    -- TODO: weird distribution of labour between mons and moninfo, can this be
    -- cleaned up?
    -- TODO: does it make sense to use the same item notability function here?
    local feats = util.map(function (i) return "item:" .. i.name() end,
                    util.filter(explorer.item_notable, mons.get_inventory()))
    local force_notable = false
    if explorer.mons_feat_filter then
        feats = util.map(explorer.mons_feat_filter, feats) -- TODO don't brute force this
    end
    if mons.type_name == "dancing weapon" and #feats > 0 then
        -- don't repeat the dancing weapon's item, but if the item_notable check
        -- has deemed the item notable, make sure to show it.
        feats = { }
        force_notable = true
    end

    -- may or may not ever be useful -- builder OOD is an indicator of builder
    -- choices, not intrinsic monster quality, and it is possible for monsters
    -- to be chosen as OOD that are within range, e.g. you can sometimes get
    -- the same monster generating as OOD and not OOD at the same depth.
    -- if mons.has_prop("mons_is_ood") then
    --     feats[#feats + 1] = "builder OOD"
    -- end

    if explorer.rare_ood(mi) then
        feats[#feats + 1] = "OOD"
    end
    if not mons.in_local_population and not explorer.mons_always_native(mons, mi)
        and not util.set(explorer.portal_order)[you.branch()] then
        -- this is a different sense of "native" than used internally to crawl,
        -- in that it includes anything that would normally generate in a
        -- branch. The internal sense is just supposed to be about which
        -- monsters "live" in a particular branch, which is covered as well.
        --
        -- portals are ignored because their monsters are mostly determined by
        -- vaults, so too many things show up as "non-native".
        feats[#feats + 1] = "non-native"
    end
    if explorer.mons_feat_filter then
        feats = util.map(explorer.mons_feat_filter, feats)
    end
    -- the `item:` prefix is really only there for filtering purposes, remove it
    feats = util.map(function (s)
            return s:find("item:") == 1 and s:sub(6) or s
        end, feats)
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
    if (force_notable or explorer.mons_notable(mons) or #feats > 0) then
        return name_string .. feat_string
    else
        return nil
    end
end

function explorer.catalog_monsters_setup()
    wiz.identify_all_items()
end

function explorer.catalog_monsters(pos, notable)
    local mons = dgn.mons_at(pos.x, pos.y)
    if mons then
        notable[#notable + 1] = explorer.describe_mons(mons)
    end
end

------------------------------
-- code for looking at vaults

function explorer.catalog_vaults()
    -- this list has already been joined into a string, split it
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

function explorer.catalog_vaults_raw()
    return { debug.vault_names() }
end

------------------------------
-- general code for building highlight descriptions

-- could do this by having each category register itself...
explorer.catalog_funs =     {vaults    = explorer.catalog_vaults,
                             vaults_raw= explorer.catalog_vaults_raw,
                             items     = explorer.catalog_items_setup,
                             monsters  = explorer.catalog_monsters_setup,
                             features  = explorer.catalog_features_setup}

explorer.catalog_pos_funs = {items     = explorer.catalog_items,
                             monsters  = explorer.catalog_monsters,
                             features  = explorer.catalog_features}

explorer.catalog_names =    {vaults    = "   Vaults: ",
                             vaults_raw= "   Vaults: ",
                             items     = "    Items: ",
                             monsters  = " Monsters: ",
                             features  = " Features: "}

-- fairly subjective, some of these should probably be relative to depth
explorer.dangerous_monsters = {
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

function explorer.reset_to_defaults()
    --explorer.mons_feat_filter = explorer.feat_in_set(util.set({ "OOD" }))
    explorer.mons_feat_filter = nil
    explorer.mons_notable = explorer.mons_ignore_boring

    explorer.always_interesting_mons = util.set(explorer.dangerous_monsters)
    explorer.item_notable = explorer.item_ignore_boring
    explorer.feat_notable = explorer.feat_interesting
end

function explorer.make_highlight(h, key, hide_empty)
    if not h[key] then
        return ""
    end
    local name = explorer.catalog_names[key]
    local l = h[key]
    if key ~= "vaults" then
        l = explorer.collapse_dups(l)
    end
    s = explorer.fancy_join(l, #name, 80, ", ")
    if (#s == 0 and hide_empty) then
        return ""
    end
    return name .. s
end

function explorer.catalog_all_positions(cats, highlights)
    funs = { }
    for _, c in ipairs(cats) do
        if explorer.catalog_pos_funs[c] ~= nil then
            if highlights[c] == nil then highlights[c] = { } end
            funs[#funs + 1] = explorer.catalog_pos_funs[c]
        end
    end
    -- don't bother if there are no positional funs to run
    if #funs == 0 then return end

    local gxm, gym = dgn.max_bounds()
    for p in iter.rect_iterator(dgn.point(1,1), dgn.point(gxm-2, gym-2)) do
        for _,c in ipairs(cats) do
            if explorer.catalog_pos_funs[c] ~= nil then
                explorer.catalog_pos_funs[c](p, highlights[c])
            end
        end
    end
    for _, c in ipairs(cats) do
        if #highlights[c] == 0 then highlights[c] = nil end
    end
end

function explorer.catalog_current_place(lvl, to_show, hide_empty)
    highlights = {}
    -- setup functions and anything that doesn't require looking at positions
    for _, cat in ipairs(to_show) do
        if explorer.catalog_funs[cat] ~= nil then
            highlights[cat] = explorer.catalog_funs[cat]()
        end
    end
    -- explorer categories that collect information from map positions
    explorer.catalog_all_positions(to_show, highlights)

    -- output
    if not explorer.quiet then
        h_l = { }
        for _, cat in ipairs(to_show) do
            h_l[#h_l+1] = explorer.make_highlight(highlights, cat, hide_empty)
        end
        h_l = util.filter(function (a) return #a > 0 end, h_l)
        if #h_l > 0 or not hide_empty then
            out(lvl .. " highlights:")
            for _, h in ipairs(h_l) do
                out(h)
            end
        end
    end
    return highlights
end

function explorer.catalog_place(i, lvl, cats_to_show, show_level_fun)
    local result = nil
    if (dgn.br_exists(string.match(lvl, "[^:]+"))) then
        debug.goto_place(lvl)
        debug.generate_level()
        local old_quiet = explorer.quiet
        if show_level_fun ~= nil and not show_level_fun(i) then
            explorer.quiet = true
        end
        result = explorer.catalog_current_place(lvl, cats_to_show, true)
        explorer.quiet = old_quiet
    end
    return result
end

function explorer.catalog_portals(i, lvl, cats_to_show, show_level_fun)
    local result = { }
    current_where = you.where()
    for j,port in ipairs(explorer.portal_order) do
        if you.where() == dgn.level_name(dgn.br_entrance(port)) then
            result[port] = explorer.catalog_place(-j, port, cats_to_show, show_level_fun)
            -- restore the level, in case there are multiple portals from a
            -- single level
            debug.goto_place(current_where)
        end
    end
    return result
end

-- a bit redundant with mapstat?
function explorer.catalog_dungeon(max_depth, cats_to_show, show_level_fun)
    local result = {}
    dgn.reset_level()
    debug.flush_map_memory()
    debug.dungeon_setup()
    for i,lvl in ipairs(explorer.generation_order) do
        if i > max_depth then break end
        result[lvl] = explorer.catalog_place(i, lvl, cats_to_show, show_level_fun)
        local portals = explorer.catalog_portals(i, lvl, cats_to_show, show_level_fun)
        for port, cat in pairs(portals) do
            result[port] = cat
        end
        -- if (dgn.br_exists(string.match(lvl, "[^:]+"))) then
        --     debug.goto_place(lvl)
        --     debug.generate_level()
        --     local old_quiet = explorer.quiet
        --     if show_level_fun ~= nil and not show_level_fun(i) then
        --         explorer.quiet = true
        --     end
        --     result[lvl] = explorer.catalog_current_place(lvl, cats_to_show, true)
        --     explorer.quiet = old_quiet
        -- end
    end
    return result
end

function explorer.catalog_seed(seed, depth, cats, show_level_fun, describe_cat)
    seed_used = debug.reset_rng(seed)
    if describe_cat then
        out(describe_cat(seed))
    else
        out("Catalog for seed " .. seed ..
            " (" .. table.concat(cats, ", ") .. "):")
    end
    return explorer.catalog_dungeon(depth, cats, show_level_fun)
end

explorer.internal_categories = { "vaults_raw" }
explorer.available_categories = { "vaults", "items", "features", "monsters" }
function explorer.is_category(c)
    return util.set(explorer.available_categories)[c] or
           util.set(explorer.internal_categories)[c]
end

-- `seeds` is an array of seeds. If you pass numbers in with this, keep in mind
-- that the max int limit is rather complicated and less than 64bits, because
-- these are doubles behind the scenes. This code will accept strings of digits
-- instead, which is always safe.
function explorer.catalog_seeds(seeds, depth, cats, show_level_fun, describe_cat)
    if depth == nil then depth = #explorer.generation_order end
    if cats == nil then
        cats = util.set(explorer.available_cats)
    end
    for _, i in ipairs(seeds) do
        if _ > 1 then out("") end -- generates a newline for stderr output
        explorer.catalog_seed(i, depth, cats, show_level_fun, describe_cat)
    end
end

-- example custom catalog function
function explorer.catalog_arts(seeds, depth)
    explorer.item_notable = explorer.arts_only
    local run1 = explorer.catalog_dungeon(seeds, depth, { "items" },
        function(seed) out("Artefact catalog for seed " .. seed .. ":") end)
    explorer.item_notable = explorer.ignore_boring
end

explorer.reset_to_defaults()
