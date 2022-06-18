-- test to exercise seed explorer: look at the first 5 levels of 5 seeds.

crawl_require('dlua/explorer.lua')

explorer.quiet = true

-- to use a >32bit seed, you will need to use a string here. If you use a
-- string, `fixed_seed` is maxed at 1.
local starting_seed = 1   -- fixed seed to start with, number or string
local fixed_seeds = 5     -- how many fixed seeds to run? Can be 0.
local rand_seeds = 0      -- how many random seeds to run

explorer.quiet = true

-- this variable determines how deep in the generation order to go
--local max_depth = #explorer.generation_order
local max_depth = 5

if fixed_seeds > 0 then
    local seed_seq = { starting_seed }

    if (type(starting_seed) ~= "string") then
        for i=starting_seed + 1, starting_seed + fixed_seeds - 1 do
            seed_seq[#seed_seq + 1] = i
        end
    end
    explorer.catalog_seeds(seed_seq, max_depth, explorer.available_categories)
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
    if not explorer.quiet then
        crawl.stderr("Exploring " .. rand_seeds .. " random seed(s).")
    end
    explorer.catalog_seeds(rand_seq, max_depth, explorer.available_categories)
end
