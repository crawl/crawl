-- Script for automated / bulk vault generation tests

-- Use case:
    
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
--     itself:
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
--    In this case the problem is pretty visible, though sometimes
--    connectivity issues do not pop out in the test output (e.g. ones
--    involving water, or CLEAR tiles, etc.) and need further debugging.

local basic_usage = [=[
Usage: util/fake_pty ./crawl -script placement.lua (<maps_to_test>|-all) [-count <n>] [-des <des_file>] [-fill] [-dump] [-force]
    Vault placement testing script. Places a vault in an empty level and test
    connectivity. A vault will fail if fails to place, or if it breaks
    connectivity. This script cannot handle all types of maps, e.g. encompass
    vaults are skipped.

    <maps_to_test>:  a list of vault names to test, or -all to test all vaults
    -count <n>:      the number of iterations to test. Default: 1.
    -des <des_file>: a des file to load, if not specified in dat/dlua/loadmaps.lua
    -fill:           use a filled level as the background. Useful for debugging
                     CLEAR issues.
    -dump:           write placed maps out to a file, named with the vault name
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

if #args_init == 0 and args["-all"] == nil then
    usage_error("\nMissing vault to test!")
end

local maps_to_test = args_init

-- Which des file is the map in? Only neeed if if the des file isn't specificed
-- in dat/dlua/loadmaps.lua
local des_file = one_arg(args, "-des", "")
local need_to_load_des = des_file ~= ""

-- How many times should we generate?
local checks = one_arg(args, "-count", 1)

-- Prefix for output files
-- placement-map.1, placement-map.2, etc.
local output_to_base = "placement-"

local dump = args["-dump"] ~= nil
local force = args["-force"] ~= nil
local fill_level = args["-fill"] ~= nil

local force_skip = util.set{
    -- skip these because they will veto unless their rune has been collected.
    -- TODO: give the player these runes?
    "uniq_cerebov", "uniq_gloorx_vloq", "uniq_mnoleg", "uniq_lom_lobon",
    -- Ignacio similarly uses some rune-based logic to randomly veto.
    "uniq_ignacio",
    -- this one has a validation check that always fails in test cases(??)
    "gauntlet_exit_mini_maze",
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

    crawl.stderr("Testing map '" .. map_to_test .."'")

    debug.builder_ignore_depth(true)
    for iter_i = 1, checks do
        output_to = output_to_base .. map_to_test .. "." .. iter_i .. ".txt"
        debug.flush_map_memory()
        dgn.reset_level()
        if not fill_level then
            dgn.fill_grd_area(1, 1, dgn.GXM - 2, dgn.GYM - 2, 'floor')
        end
        -- TODO: are there any issues that using these tags will hide? I'm
        -- pretty sure the rotate/mirror ones are ok, not sure about water
        dgn.tags(map, "no_rotate no_vmirror no_hmirror no_pool_fixup")
        if not dgn.place_map(map, true, true) then
            local e = "Failed to place '" .. map_to_test .. "'"
            local last_error = dgn.last_builder_error()
            if last_error and #last_error > 0 then
              e = e .. "; last builder error: " .. last_error
            end
            assert(false, e)
        end
        local z = dgn.count_disconnected_zones()
        if dump then
            debug.dump_map(output_to)
            crawl.message("   Placed " .. map_to_test .. ":" .. iter_i .. ", dumping to " .. output_to)
        end
        if z ~= 1 then
            local connectivity_err = "Isolated area in vault " .. map_to_test .. " (" .. z .. " zones)"
            if dump then
                crawl.stderr("    Failing vault output to: " .. output_to)
            end
            if force_connectivity_ok[map_to_test] then
                crawl.stderr("(Force skipped) " .. connectivity_err)
            else
                assert(z == 1, connectivity_err)
            end
        end
    end
    debug.builder_ignore_depth(false)
end


local function generate_maps()
    if need_to_load_des then
        dgn.load_des_file(des_file)
    end

    if #maps_to_test > 0 then
        for _,map_to_test in ipairs(maps_to_test) do
            local map = dgn.map_by_name(map_to_test)
            if not map then
                assert(false, "Couldn't find the map named " .. map_to_test)
            end
            generate_map(map)
        end
    else
        -- -all
        crawl.stderr("Testing " .. dgn.map_count() .. " maps")
        for i = 0, dgn.map_count()-1 do
            local map = dgn.map_by_index(i)
            if not map then
                assert(false, "invalid map at index " .. i)
            end
            --crawl.stderr(i)
            generate_map(map)
        end
    end
end

generate_maps()
