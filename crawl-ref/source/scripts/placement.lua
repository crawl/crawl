-- Script for automated / bulk vault generation tests

-- run with no arguments or --help to see all command line options:
--     `util/fake_pty ./crawl -script placement.lua --help`

-- Example use case:
--     ```
--     $ util/fake_pty ./crawl -script placement.lua minmay_nonomino_d4 -count 10 -dump
--     Testing map 'minmay_nonomino_d4'
--         Failing vault output to: placement-minmay_nonomino_d4.10.txt
--     script error: ./scripts/placement.lua:184: Isolated area in vault minmay_nonomino_d4 (2 zones)
--     ```

--     This generates this vault on a blank level 10 times and outputs each map
--     to a file. In this case, it failed on try 10 (which aborts) and so it
--     stops then and points out the specific failing file to you. This file
--     shows the whole level, but here's an excerpt illustrating the vault
--     itself, at one point in its history:
--
-- ```
-- #..............................####.####.......................................#
-- #..............................#.#...#.#.......................................#
-- #..............................##.....##.......................................#
-- #..............................#..###..#.......................................#
-- #.................................#.#..........................................#
-- #..............................#..###..#.......................................#
-- #..............................##.....##.......................................#
-- #..............................#.#...#.#.......................................#
-- #..............................####.####.......................................#
-- ```
--
--    In this case the problem is pretty visible (the closet in the middle),
--    though under some conditions a connectivity issue may not be visible in
--    the text output (when it involves shallow vs. deep water, CLEAR tiles,
--    etc.) In these cases one strategy is to tweak the vault itself so that
--    traversable tiles do show up as floor, and rerun placement.lua.

local basic_usage = [=[
Usage: util/fake_pty ./crawl -script placement.lua <maps_to_test> [-all] [-nmaps <n>] [-count <n>] [-des <des_file>] [-fill] [-opacity] [-tele-zones] [-dump] [-log] [-force]
    Vault placement testing script. Places a vault in an empty level and test
    connectivity. A vault will fail if fails to place, or if it breaks
    connectivity. This script cannot handle all types of maps, e.g. encompass
    vaults are skipped.

    <maps_to_test>:  a list of vault names to test. Either at least one map
                     name, or `-all`, must be specified.
    -all:            test all maps. If a map name is given, skip all maps in
                     order up to that vault. (If multiple vault names are
                     given, ignores all but the first.) This option must follow
                     any map names.
    -nmaps <n>:      in combination with -all, how many maps to test. If <= 0
                     or unspecified, tests all remaining maps. Ignored without
                     -all.
    -count <n>:      the number of iterations to test. Default: 1.
    -des <des_file>: a des file to load, if not specified in dat/dlua/loadmaps.lua
    -fill:           use a filled level as the background. Useful for debugging
                     CLEAR issues. However, minivaults will always fail.
    -opacity:        test opaque vaults, forcing them to have a `transparent`
                     tag. The main use of this is to find vaults that should
                     be marked transparent. In this mode, 1-zone opaque maps
                     will produce a message (as opposed to multi-zone maps
                     in other modes). Overrides all options that normally
                     target multi-zone connectivity checking.
    -tele-zones:     Search for zones that are identifiable as teleport closets,
                     based on no_tele_into tags and masking. This is especially
                     useful for cases where the regular connectivity checks
                     will pass the vault, e.g. non-transparent vaults.
    -dump:           write placed maps out to a file, named with the vault name
    -log:            Append message log to dump output; requires a fulldebug
                     build to be most useful. Entails -dump.
    -force:          place maps that would normally be skipped by this script
                     (i.e. `unrand` tagged vaults, encompass vaults, etc). This
                     will often cause errors.
]=]

function parse_args(args, err_fun)
    accum_init = { }
    accum_params = { }
    cur = nil
    for _,a in ipairs(args) do
        if string.find(a, '-') == 1 then
            cur = a
            if accum_params[a] ~= nil then err_fun("Repeated argument '" .. a .."'") end
            accum_params[a] = { }
        else
            if cur == nil then
                accum_init[#accum_init + 1] = a
            else
                accum_params[cur][#accum_params[cur] + 1] = a
            end
        end
    end
    return accum_init, accum_params
end

function one_arg(args, a, default)
    if args[a] == nil or #(args[a]) ~= 1 then return default end
    return args[a][1]
end

function usage_error(extra)
    local err = basic_usage
    if extra ~= nil then
        err = err .. "\n" .. extra
    end
    script.usage(err)
end

local arg_list = crawl.script_args()
local args_init, args = parse_args(arg_list, usage_error)

if args["-help"] ~= nil then script.usage(basic_usage) end

local requested_all = args["-all"] ~= nil

if #args_init == 0 and not requested_all then
    usage_error("\nMissing vault to test!")
end

local maps_to_test = args_init

-- Which des file is the map in? Only need if if the des file isn't specified
-- in dat/dlua/loadmaps.lua
local des_file = one_arg(args, "-des", "")
local need_to_load_des = des_file ~= ""

-- How many times should we generate?
local checks = one_arg(args, "-count", 1)

local nmaps = one_arg(args, "-nmaps", -1)

-- Prefix for output files
-- placement-map.1, placement-map.2, etc.
local output_to_base = "placement-"

local force = args["-force"] ~= nil

local tele_zones = args["-tele-zones"] ~= nil
local opacity = args["-opacity"] ~= nil

-- fill_level will fail all minivaults, because their connectivity check fails.
-- In principle, this could be changed in maps.cc:_find_minivault_place, by only
-- doing the _connected_minivault_place check if check_place is true, but this
-- is a pretty worryingly low-level change; in my testing, making this change
-- will interfere with Vaults layouts in ways that I don't understand.
local fill_level = args["-fill"] ~= nil
local dump = args["-dump"] ~= nil
local builder_log = args["-log"] ~= nil
if builder_log then dump = true end

local force_skip = util.set{
    -- skip these because they will veto unless their rune has been collected.
    -- TODO: give the player these runes?
    "uniq_cerebov", "uniq_gloorx_vloq", "uniq_mnoleg", "uniq_lom_lobon",
    -- Ignacio similarly uses some rune-based logic to randomly veto.
    "uniq_ignacio",
    -- this one has a validation check that always fails in test cases(??)
    "gauntlet_exit_mini_maze",
    "pf_just_have_faith", -- uses mimics that this code fails to detect
    -- the following use transporters which are not yet handled by this script
    "elyvilon_altar_4",
    "gammafunk_temple_of_torment",
    -- This is broken by the testing method in this script whereby a is placed
    -- at (1,1) because - because the vault has orient NW, this frequently
    -- creates a closet. Not obvious how to properly resolve this.
    "evilmike_arrival_grusome_pit",
    -- It's Abyss, they're *supposed* to be closets
    "evilmike_abyss_exit_glass",
    "evilmike_abyss_exit_10",
    "evilmike_abyss_exit_12",
    "evilmike_abyss_exit_15",
    "evilmike_abyss_exit_kraken",
    "guppyfry_abyss_exit_imp_island",
    "spicy_abyss_rude_harpoons",

}

local force_connectivity_ok = util.set{
    -- These heavily use blank spaces because of abyss placement, in ways that
    -- won't work under normal constraints. TODO: just skip connectivity checks
    -- for abyss vaults? But the lua infrastructure isn't implemented yet
    "hangedman_abyss_lost_library", "hangedman_abyss_batty_box",
    "hangedman_relentless_pull", "hangedman_steaming_rock",
    "hangedman_illusive_loot", "hangedman_abyss_town",
    -- similar but for vaults placement:
    -- (TODO: would be good to double check that these do place in vaults...)
    "nicolae_vaults_bisected_inception",
    "nicolae_vaults_inception_statues",
    "nicolae_vaults_inception_scallops",
    "nicolae_vaults_inception_network",
    "nicolae_vaults_inception_corners",
    -- vaults that use vetoes to downweight the vault or deal with consequences
    -- of brute force randomization. In principle this should be done some
    -- other way, but skip for now.
    "minmay_lair_entry_lava", -- vetoes around 80-85% of the time
    -- super complicated randomization that can generate pockets, I haven't
    -- figured it out enough to do justice to fixing:
    "hangedman_spider_tarantella_strand"
}

-- place a single vault in an empty level
local function generate_map(map)
    map_to_test = dgn.name(map)

    if opacity and dgn.has_tag(map, "transparent") then
        return
    end

    if not force then
        if dgn.has_tag(map, "removed") then
            crawl.stderr("Skipping removed map '" .. map_to_test .. "'")
            return
        end

        if force_skip[map_to_test] then
            crawl.stderr("Force skipping map '" .. map_to_test .. "'")
            return
        end

        -- it would be nice to automatically test some of these, but there are
        -- various cases that just present really intractable problems for placing
        -- in a test. So you'll need to do them manually for now.
        if dgn.has_tag(map, "unrand") then
            crawl.stderr("Skipping unrand map '" .. map_to_test .. "'")
            return
        end
        -- TODO: place these somehow? Need to override the check on interlevel
        -- connectivity, at least. Some overlap with previous check.
        if dgn.orient(map) == "encompass"
                and string.find(dgn.place(map), "%$") ~= nil then
            crawl.stderr("Skipping branch end '" .. map_to_test .. "'")
            return
        end
        -- TODO: place non-end encompass maps at least
        if dgn.orient(map) == "encompass" then
            crawl.stderr("Skipping encompass map '" .. map_to_test .. "'")
            return
        end
    end

    if not opacity then
        crawl.stderr("Testing vault '" .. map_to_test .."'")
    end

    debug.builder_ignore_depth(true)
    local max_zones = 0
    for iter_i = 1, checks do
        output_to = output_to_base .. map_to_test .. "." .. iter_i .. ".txt"
        debug.reset_player_data()
        dgn.reset_level()
        crawl.clear_message_store()
        crawl.mpr("Testing vault '" .. map_to_test .. "', iteration " .. iter_i .. " of " .. checks)
        if not fill_level then
            dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')
        end
        -- TODO: are there any issues that using these tags will hide? I'm
        -- pretty sure the rotate/mirror ones are ok, not sure about water
        dgn.tags(map, "no_rotate no_vmirror no_hmirror no_pool_fixup")
        if (opacity) then
            dgn.tags(map, "transparent")
        end
        if not dgn.place_map(map, false, true) then
            local e = "Failed to place '" .. map_to_test .. "'"
            local last_error = dgn.last_builder_error()
            if last_error and #last_error > 0 then
              e = e .. "; last builder error: " .. last_error
            end
            if dump then
                -- even if the map is empty, the log could contain useful info
                debug.dump_map(output_to, builder_log)
            end
            assert(false, e)
        end
        local z = dgn.count_disconnected_zones()
        max_zones = math.max(z, max_zones)
        if dump then
            debug.dump_map(output_to, builder_log)
            crawl.message("   Placed " .. map_to_test .. ":" .. iter_i .. ", dumping to " .. output_to)
        end

        if not opacity then
            if z ~= 1 then
                local connectivity_err = "Isolated area in vault " .. map_to_test .. " (" .. z .. " zones) from file " .. dgn.filename(map) .. " on attempt #" .. iter_i
                if dump then
                    crawl.stderr("    Failing vault output to: " .. output_to)
                end
                if force_connectivity_ok[map_to_test] then
                    crawl.stderr("(Force skipped) " .. connectivity_err)
                else
                    assert(z == 1, connectivity_err)
                end
            end
            if tele_zones then
                -- we need some kind of stairs so that vaults that place no stairs
                -- don't count as a closet. We can't assume a baseline of 1 because
                -- some vaults *do* place stairs, in which case an additional
                -- closet will count as 1. Unclear if this position works for all
                -- vaults...
                dgn.fill_grd_area(1, 1, 1, 1, 'stone_stairs_up_i')
                z = dgn.count_tele_zones()
                if dump then
                    -- just rewrite it
                    debug.dump_map(output_to, builder_log)
                end
                if z >= 1 then
                    local connectivity_err = "Teleport closet in vault " .. map_to_test .. " (" .. z .. " zones) from file " .. dgn.filename(map) .. " on attempt #" .. iter_i
                    if dump then
                        crawl.stderr("    Failing vault output to: " .. output_to)
                    end
                    -- anything that has connectivity problems will have tele
                    -- closets, skip the same list
                    if force_connectivity_ok[map_to_test] then
                        crawl.stderr("(Force skipped) " .. connectivity_err)
                    else
                        assert(z == 0, connectivity_err)
                    end
                end
            end
        end
    end
    if opacity and max_zones == 1 then
        crawl.stderr("1-zone opaque map " .. map_to_test .. " from file " .. dgn.filename(map))
    end
    debug.builder_ignore_depth(false)
end


local function generate_maps()
    if need_to_load_des then
        dgn.load_des_file(des_file)
    end

    if #maps_to_test > 0 and not requested_all then
        for _,map_to_test in ipairs(maps_to_test) do
            local map = dgn.map_by_name(map_to_test)
            if not map then
                assert(false, "Couldn't find the map named " .. map_to_test)
            end
            generate_map(map)
        end
    else
        -- -all
        local start = -1
        if #maps_to_test > 0 then
            if #maps_to_test > 1 then
                crawl.stderr("Warning: ignoring multiple vault names with -all")
            end
            local first_map = dgn.map_by_name(maps_to_test[1])

            assert(first_map, "Couldn't find the map named " .. maps_to_test[1])
            crawl.stderr("Skipping to " .. dgn.name(first_map))
            for i = 0, dgn.map_count()-1 do
                if dgn.name(dgn.map_by_index(i)) == dgn.name(first_map) then
                    start = i
                    break
                end
            end
            assert(start > 0, "Couldn't find map in index: " .. dgn.name(first_map))
        end
        if start < 0 then start = 0 end
        if tonumber(nmaps) <= 0 then nmaps = dgn.map_count() - start end
        nmaps = math.min(nmaps, dgn.map_count() - start)

        crawl.stderr("Testing " .. nmaps .. " maps")
        local last_map = ""
        for i = start, start + nmaps - 1 do
            local map = dgn.map_by_index(i)
            if not map then
                assert(false, "invalid map at index " .. i)
            end
            last_map = dgn.name(map)
            -- crawl.stderr(i)
            generate_map(map)
        end
        -- to make resuming easier
        if opacity and (start > 0 or nmaps < dgn.map_count() - start) then
            crawl.stderr("Last map: " .. last_map)
        end
    end
end

generate_maps()
