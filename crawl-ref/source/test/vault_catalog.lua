-- this test checks for the stability of vault generation given various
-- particular seeds. It will print out a list of all vaults for one particular
-- seed, which can be useful for comparing across devices, and test stability
-- of seeds by rerunning the catalog for various seeds. The default settings
-- are relatively moderate in terms of time/CPU requirements for the sake of
-- travis CI.
--
-- this test messes with RNG state! It leaves the RNG with a clean seed 1,
-- but if you are expecting deterministic test behavior, take that into
-- account.

crawl_require('dlua/explorer.lua')

local starting_seed = 1   -- fixed seed to start with
local fixed_seeds = 1     -- how many fixed seeds to run?
local per_seed_iters = 1  -- how many stability tests to run per seed
local rand_seeds = 1      -- how many random seeds to run

local max_depth = #explorer.generation_order

function catalog_dungeon_vaults(silent)
    -- TODO: this doesn't do any pan levels, but the previous version did
    local old_quiet = explorer.quiet
    explorer.quiet = silent
    local catalog = explorer.catalog_dungeon(max_depth,
                                             { "vaults_raw" })
    explorer.quiet = old_quiet
    local result = { }
    for lvl, cats in pairs(catalog) do
        -- eliminate the extra catalog structure we aren't using here
        result[lvl] = catalog[lvl]["vaults_raw"][1]
    end
    return result
end

function compare_catalogs(run1, run2, seed)
    local diverge = false
    -- stop at the first divergence by generation order. Everything after that
    -- is likely to be a mess with very little signal.
    for i,lvl in ipairs(explorer.generation_order) do
        if run1[lvl] == nil and run2[lvl] ~= nil then
            crawl.stderr("Run 1 is missing " .. lvl .. "!")
        elseif run2[lvl] == nil and run1[lvl] ~= nil then
            crawl.stderr("Run 2 is missing " .. lvl .. "!")
        elseif run1[lvl] ~= run2[lvl] then
            crawl.stderr("Runs diverge for seed " .. seed .. " on level " .. lvl .. "!")
            crawl.stderr("Run 1: " .. run1[lvl])
            crawl.stderr("Run 2: " .. run2[lvl])
        end
        assert(run1[lvl] == run2[lvl])
    end
end

function test_seed(seed, iters,  quiet)
    seed_used = debug.reset_rng(seed)
    if quiet then
        crawl.stderr(".")
    else
        crawl.stderr("Vault catalog for seed " .. seed .. ":")
    end
    local run1 = catalog_dungeon_vaults(quiet)

    if not quiet then
        crawl.stderr("....now testing vault generation stability for seed " ..seed.. ".")
    end

    for i = 1,iters do
        crawl.stderr(".")
        debug.reset_rng(seed)
        local run2 = catalog_dungeon_vaults(true)
        compare_catalogs(run1, run2, seed)
    end
end

function test_seed_sequence(seq, iters, quiet, quiet_first_only)
    for _,s in ipairs(seq) do
        crawl.stderr("Testing seed " .. s .. ".")
        test_seed(s, iters, quiet or quiet_first_only and s ~= seq[1])
    end
end

if fixed_seeds > 0 then
    local seeds_to_test = { starting_seed }
    if (type(starting_seed) ~= "string") then
        for i=starting_seed + 1, starting_seed + fixed_seeds - 1 do
            seeds_to_test[#seeds_to_test + 1] = i
        end
    end
    test_seed_sequence(seeds_to_test, per_seed_iters, quiet)
end

-- shorten the depth here for the sake of travis:
max_depth = explorer.zot_depth

if rand_seeds > 0 then
    crawl.stderr("Testing " .. rand_seeds .. " random seed(s).")
    local seeds_to_test = { }
    for i=1,rand_seeds do
        -- intentional use of non-crawl random(). random doesn't seem to accept
        -- anything bigger than 32 bits for the range.
        math.randomseed(crawl.millis())
        rand_seed = math.random(0x7FFFFFFF)
        seeds_to_test[#seeds_to_test + 1] = rand_seed
    end
    test_seed_sequence(seeds_to_test, per_seed_iters, true)
end

debug.reset_rng(1)
