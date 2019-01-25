-- this test prints out the D:1 vaults for some test seeds. This can be used
-- to diagnose when seeding changes or isn't working right.

-- this test messes with RNG state! It leaves the RNG with a clean seed 1,
-- but if you are expecting deterministic test behavior, take that into
-- account.

local eol = string.char(13)

debug.reset_rng(1)
crawl.stderr("Here are 10 random numbers from seed 1:" .. eol)
for i = 1,10 do
    crawl.stderr(crawl.random2(100) .. " ")
end
crawl.stderr(eol)

for seed = 1,10 do
    debug.reset_rng(seed)
    crawl.stderr("       initial rng: " .. debug.get_rng_state() .. eol)
    -- debug.enter_dungeon() crashes, for some reason
    -- the following runs through RNG state in the same way that starting a
    -- regular game would -- if it produces different results, that's a bug.
    debug.flush_map_memory()
    debug.dungeon_setup()
    crawl.stderr("   after dgn setup: " .. debug.get_rng_state() .. eol)
    debug.goto_place("D:1")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("D:1 vaults for seed " .. seed .. " are: " .. vault_names .. eol)
    crawl.stderr("     after D:1 gen: " .. debug.get_rng_state() .. eol)
    debug.goto_place("Temple")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("Temple vaults for seed " .. seed .. " are: " .. vault_names .. eol)

end

debug.reset_rng(1)
