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

local starting_seed = 1   -- fixed seed to start with
local fixed_seeds = 2     -- how many fixed seeds to run?
local per_seed_iters = 1  -- how many stability tests to run per seed
local rand_seeds = 1      -- how many random seeds to run

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
            }
                -- pregen continues: tartarus, cocytus, dis, geh

-- want to exercise these a bit
local generation_order_extended = {
                "D:1",
                "Tar:1", "Tar:2", "Tar:3", "Tar:4", "Tar:5", "Tar:6", "Tar:7",
                "Coc:1", "Coc:2", "Coc:3", "Coc:4", "Coc:5", "Coc:6", "Coc:7",
                "Dis:1", "Dis:2", "Dis:3", "Dis:4", "Dis:5", "Dis:6", "Dis:7",
                "Pan", "Pan", "Pan", "Pan", "Pan", "Pan", "Pan"
}

-- a bit redundant with mapstat? but simpler...
function catalog_dungeon_vaults(level_order, silent)
    local result = {}
    debug.flush_map_memory()
    debug.dungeon_setup()
    for i,lvl in ipairs(level_order) do
        if (dgn.br_exists(string.match(lvl, "[^:]+"))) then
            debug.goto_place(lvl)
            debug.generate_level()
            result[lvl] = debug.vault_names()
            if (not silent) then
                crawl.stderr(lvl .. " vaults are: ")
                crawl.stderr("    " .. result[lvl])
            end
        end
    end
    return result
end

function compare_catalogs(order, run1, run2, seed)
    local diverge = false
    -- stop at the first divergence by generation order. Everything after that
    -- is likely to be a mess with very little signal.
    for i,lvl in ipairs(order) do
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

function test_seed(seed, iters, order, quiet)
    debug.reset_rng(seed)
    if quiet then
        crawl.stderr(".")
    else
        crawl.stderr("Vault catalog for seed " .. seed .. ":")
    end
    local run1 = catalog_dungeon_vaults(order, quiet)

    if not quiet then
        crawl.stderr("....now testing vault generation stability for seed " ..seed.. ".")
    end

    for i = 1,iters do
        crawl.stderr(".")
        debug.reset_rng(seed)
        local run2 = catalog_dungeon_vaults(order, true)
        compare_catalogs(order, run1, run2, seed)
    end
end

test_seed(starting_seed, per_seed_iters, generation_order, false)
-- just do a quick check of hell / pan stability
crawl.stderr("Checking some extended branches for seed " .. starting_seed .. "...")
test_seed(starting_seed, per_seed_iters, generation_order_extended, true)

for i=starting_seed + 1, starting_seed + fixed_seeds - 2 do
    crawl.stderr("Testing seed " .. i .. ".")
    test_seed(i, per_seed_iters, generation_order, true)
end

if rand_seeds > 0 then
    crawl.stderr("Testing " .. rand_seeds .. " random seed(s).")
end
for i=1,rand_seeds do
    -- intentional use of non-crawl random(). random doesn't seem to accept
    -- anything bigger than 32 bits for the range.
    math.randomseed(crawl.millis())
    rand_seed = math.random(0x7FFFFFFF)
    crawl.stderr("Testing random seed " .. rand_seed .. ".")
    test_seed(rand_seed, per_seed_iters, generation_order, true)
end
