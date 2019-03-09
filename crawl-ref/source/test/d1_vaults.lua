-- this test prints out the D:1-D:4 vaults for some test seeds. This can be used
-- to diagnose when seeding changes or isn't working right.

-- this test messes with RNG state! It leaves the RNG with a clean seed 1,
-- but if you are expecting deterministic test behavior, take that into
-- account.

local eol = string.char(13)

for seed = 1,10 do
    crawl.stderr("Seed: " .. seed .. eol)
    debug.reset_rng(seed)
    -- debug.enter_dungeon() crashes, for some reason
    -- the following runs through RNG state in the same way that starting a
    -- regular game would -- if it produces different results, that's a bug.
    debug.flush_map_memory()
    debug.dungeon_setup()
    debug.goto_place("D:1")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("    D:1 vaults for seed " .. seed .. " are: " .. vault_names .. eol)
    debug.goto_place("D:2")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("    D:2 vaults for seed " .. seed .. " are: " .. vault_names .. eol)
    debug.goto_place("D:3")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("    D:3 vaults for seed " .. seed .. " are: " .. vault_names .. eol)
    debug.goto_place("D:4")
    debug.generate_level()
    local vault_names = debug.vault_names()

    crawl.stderr("    D:4 vaults for seed " .. seed .. " are: " .. vault_names .. eol)
end

debug.reset_rng(1)
