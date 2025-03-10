-- test mutation + species. Requires debug mode to fully validate mutation states.
-- the most important case to test here is demonspawn.

local silent = true -- change to false to look at the result of stress tests
local ds_mut_iterations = 250 -- mutate each try this many times
local ds_tries = 30
local mut_iterations = 100
local tries = 5

local chance_temporary = 10 -- in 100.
local chance_clear = 2 -- in 100. Chance to clear some temporary mutations. Note that some mutations clear others anyways.

local eol = string.char(13)

local function give_random_mutation(chance_temporary)
    local temp = crawl.x_chance_in_y(chance_temporary, 100)
    you.mutate("any", "mutation tests", temp)
end

local function random_level_change(species, tries, iterations, chance_temporary, chance_clear)
    if not silent then
        crawl.stderr("###### begin random level change test for " .. species .. eol)
    end
    for i=1, tries do
        crawl.dpr("############ " .. species .. " random level test try " .. i .. ", resetting to level 1")
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
    if not silent then
        crawl.stderr("###### begin species test for " .. species .. eol)
    end
    for i=1, tries do
        crawl.dpr("############ " .. species .. " mutation test try " .. i .. ", resetting to level 1")
        you.delete_all_mutations("species mutation test")
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

local function test_random_mutations_slime(species, tries, iterations, chance_temporary, chance_clear)
    if not silent then
        crawl.stderr("###### begin slime test for " .. species .. eol)
    end
    for i=1, tries do
        crawl.dpr("############ " .. species .. " slime test try " .. i .. ", resetting to level 1")
        you.delete_all_mutations("slime test")
        assert(you.set_xl(1, false))
        assert(you.change_species(species))
        for j=1, iterations do
            if you.xl() < 27 and crawl.x_chance_in_y(iterations / 27, iterations) then
                assert(you.set_xl(you.xl() + 1, false))
            end
            if crawl.x_chance_in_y(chance_clear, 100) then
                if crawl.coinflip() then
                    you.delete_temp_mutations(crawl.coinflip(), species .. " slime test")
                else
                    you.delete_mutation("slime", "mutation test")
                end
            else
                if crawl.coinflip() then
                    you.mutate("slime", "slime tests", false)
                else
                    you.mutate("any", "slime tests", crawl.x_chance_in_y(chance_temporary, 100))
                end
            end
        end
        if not silent then
            crawl.stderr(species .. " slime test try " .. i .. " final xl: " .. you.xl() ..
                " " .. you.how_mutated(true, true, true) ..
                " final mutations: " .. you.mutation_overview() .. eol)
        end
    end
end


species = {"mountain dwarf", "minotaur", "merfolk", "gargoyle", "draconian", "troll", "deep elf",
           "armataur", "gnoll", "human", "kobold", "revenant", "djinni", "spriggan", "tengu", "oni",
           "barachi", "coglin", "vine stalker", "poltergeist", "demigod", "formicid", "naga",
           "octopode", "felid", "mummy"}

local old_species = you.species()
local you_x, you_y = you.pos() -- probably out of bounds
-- move to a guaranteed real position. This is because losing some mutations
-- can trigger things like landing the player, which will crash if out of
-- bounds.
local place = dgn.point(20, 20)
dgn.grid(place.x, place.y, "floor")
you.moveto(place.x, place.y) -- assumes other tests have properly cleaned up...

test_random_mutations_species("demonspawn", ds_tries, ds_mut_iterations, chance_temporary, chance_clear)
random_level_change("demonspawn", ds_tries, ds_mut_iterations, chance_temporary, chance_clear)
test_random_mutations_slime("demonspawn", ds_tries, ds_mut_iterations, chance_temporary, chance_clear)


for i, sp_name in ipairs(species) do
    test_random_mutations_species(sp_name, tries, mut_iterations, chance_temporary, chance_clear)
end

-- the testbed doesn't really clean up much of anything.
you.delete_all_mutations("Species mutation test")
assert(you.change_species(old_species)) -- restore original player species
assert(you.set_xl(1, false))
you.moveto(you_x, you_y) -- restore original player pos
