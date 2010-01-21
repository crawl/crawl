-- Check specific map generation, useful for testing portal vault
-- generation (SUBSTs, NSBUSTs, SHUFFLEs, etc).

-- Name of the map!
local map_to_test = ""
-- Which des file is the map in?
local des_file = ""
-- Change to true if the des file isn't specificed in dat/clua/loadmaps.lua
local need_to_load_des = false
-- How many times should we generate?
local checks = 10
-- Output to this file, will append iteration to the end, ie,
-- output_to.map.1, output_to.map.2, etc.
local output_to = ""

-- Should we run these tests?
local run_test = map_to_test ~= ""

local function generate_map()
    output_to = output_to == "" and map_to_test or output_to
    if map_to_test == "" or
       (des_file == "" and need_to_load_des) or
       output_to == "" then
       assert(false, "Need a map, a des file (if not already loaded), and an output.")
    end

    if need_to_load_des then
        dgn.load_des_file(des_file)
    end

    local map = dgn.map_by_name(map_to_test)

    if not map then
        assert(false, "Couldn't find the map named " .. map_to_test)
    end

    for iter_i = 1, checks do
        debug.flush_map_memory()
        dgn.reset_level()
        dgn.tags(map, "no_rotate no_vmirror no_hmirror no_pool_fixup")
        dgn.place_map(map, true, true)
        crawl.message("Placed " .. map_to_test .. ":" .. iter_i .. ", dumping to " .. output_to .. "." .. iter_i)
        debug.dump_map(output_to .. "." .. iter_i)
    end
end

if run_test then
    generate_map()
else
    crawl.message("Not running vault generation test.")
end
