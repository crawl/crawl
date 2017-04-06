-- test demonspawn mutations. Requires debug mode to fully validate mutation states.

local silent = false -- change to false to look at the result of stress tests
local mut_iterations = 1000 -- mutate each try this many times
local tries = 40

local chance_temporary = 10 -- in 100.
local chance_clear = 2 -- in 100. Chance to clear some temporary mutations. Note that some mutations clear others anyways.

local eol = string.char(13)

local function give_random_mutation(chance_temporary)
    local temp = crawl.x_chance_in_y(chance_temporary, 100)
    you.mutate("any", "mutation test", temp)
end


local function test_random_mutations_ds(tries, iterations, chance_temporary, chance_clear)
    for i=1, tries do
        you.delete_all_mutations("Ds mutation test")
        assert(you.set_xl(1, false))
        assert(you.change_species("demonspawn"))
        for j=1, iterations do
            if you.xl() < 27 and crawl.x_chance_in_y(iterations / 27, iterations) then
                assert(you.set_xl(you.xl() + 1, false))
            end
            if crawl.x_chance_in_y(chance_clear, 100) then
                you.delete_temp_mutations(crawl.coinflip(), "Ds mutation test")
            else
                give_random_mutation(chance_temporary)
            end
        end
        if not silent then
            crawl.stderr("try " .. i .. " final xl: " .. you.xl() ..
                " " .. you.how_mutated(true, true, true) ..
                " final mutations: " .. you.mutation_overview() .. eol)
        end
    end
end

test_random_mutations_ds(tries, mut_iterations, chance_temporary, chance_clear)
