-- test mutation + species. Requires debug mode to fully validate mutation states.
-- the most important case to test here is demonspawn.

local silent = false -- change to false to look at the result of stress tests
local ds_mut_iterations = 1000 -- mutate each try this many times
local ds_tries = 30
local mut_iterations = 500
local tries = 5

local chance_temporary = 10 -- in 100.
local chance_clear = 2 -- in 100. Chance to clear some temporary mutations. Note that some mutations clear others anyways.

local eol = string.char(13)

local function give_random_mutation(chance_temporary)
    local temp = crawl.x_chance_in_y(chance_temporary, 100)
    you.mutate("any", "mutation tests", temp)
end

local function random_level_change(species, tries, iterations, chance_temporary, chance_clear)
    for i=1, tries do
        crawl.dpr("############ Ds random level test try " .. i .. ", resetting to level 1")
        you.delete_all_mutations(species .. " random level test")
        assert(you.set_xl(1, false))
        assert(you.change_species(species))
        for j=1, iterations do
            if  crawl.x_chance_in_y(iterations / 27, iterations) then
                assert(you.set_xl(crawl.random_range(1, 27), false))
            end
            if crawl.x_chance_in_y(chance_clear, 100) then
                you.delete_temp_mutations(crawl.coinflip(), species .. " random level test")
            else
                give_random_mutation(chance_temporary)
            end
        end
        if not silent then
            crawl.stderr(species .. " random level test try " .. i .. " final xl: " .. you.xl() ..
                " " .. you.how_mutated(true, true, true) ..
                " final mutations: " .. you.mutation_overview() .. eol)
        end
    end
end

local function test_random_mutations_species(species, tries, iterations, chance_temporary, chance_clear)
    for i=1, tries do
        crawl.dpr("############ " .. species .. " mutation test try " .. i .. ", resetting to level 1")
        you.delete_all_mutations("Ds mutation test")
        assert(you.set_xl(1, false))
        assert(you.change_species(species))
        for j=1, iterations do
            if you.xl() < 27 and crawl.x_chance_in_y(iterations / 27, iterations) then
                assert(you.set_xl(you.xl() + 1, false))
            end
            if crawl.x_chance_in_y(chance_clear, 100) then
                you.delete_temp_mutations(crawl.coinflip(), species .. " mutation test")
            else
                give_random_mutation(chance_temporary)
            end
        end
        if not silent then
            crawl.stderr(species .. " mutation test try " .. i .. " final xl: " .. you.xl() ..
                " " .. you.how_mutated(true, true, true) ..
                " final mutations: " .. you.mutation_overview() .. eol)
        end
    end
end

species = {"hill orc", "minotaur", "merfolk", "gargoyle", "draconian", "halfling", "troll", "ghoul", 
            "human", "kobold", "centaur", "spriggan", "tengu", "deep elf", "ogre", "deep dwarf",
            "vine stalker", "vampire", "demigod", "formicid", "naga", "octopode", "felid", "barachi",
            "mummy"}

test_random_mutations_species("demonspawn", ds_tries, ds_mut_iterations, chance_temporary, chance_clear)
random_level_change("demonspawn", ds_tries, ds_mut_iterations, chance_temporary, chance_clear)

for i, sp_name in ipairs(species) do
    test_random_mutations_species(sp_name, tries, mut_iterations, chance_temporary, chance_clear)
end

-- the testbed doesn't really clean up much of anything.
you.delete_all_mutations("Species mutation test")
assert(you.change_species("human")) -- should clean up any innate mutatinos
assert(you.set_xl(1, false))

