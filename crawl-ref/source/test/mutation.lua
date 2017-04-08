local silent = false -- change to false to look at the result of stress tests
local mut_iterations = 500 -- mutate each try this many times
local tries = 40

local chance_temporary = 10 -- in 100.
local chance_clear = 2 -- in 100. Chance to clear some temporary mutations. Note that some mutations clear others anyways.

local eol = string.char(13)

-- mostly for show at the moment -- should add more things here.
local function test_basic_mutation_stuff()
    you.delete_all_mutations("mutation test")
    you.mutate("robust", "basic mutation test", false)
    you.mutate("frail", "basic mutation test", false)
    assert(you.get_base_mutation_level("robust") == 0)
    for i=1, 10 do
        you.mutate("robust", "basic mutation test", false)
    end
    assert(you.get_base_mutation_level("robust", true, true, true) == 3)
end


local function give_random_mutation(chance_temporary)
    local temp = crawl.x_chance_in_y(chance_temporary, 100)
    you.mutate("any", "mutation test", temp)
end

local function try_all_mutation_categories()
    you.mutate("any", "mutation test", false)
    you.mutate("good", "mutation test", false)
    you.mutate("bad", "mutation test", false)
    you.mutate("xom", "mutation test", false)
    you.mutate("slime", "mutation test", false)
    you.mutate("corrupt", "mutation test", true)
    you.mutate("qazlal", "mutation test", false)
end

-- TODO: better way to do this in lua so that it doesn't duplicate code
local function simulate_mutation_pot()
    remove = crawl.random_range(2, 6)
    add = crawl.random_range(1, 3)
    for i=1, remove do
        you.delete_mutation("any", "mutation test")
    end
    for i=1, add do
        you.mutate("any", "mutation test", false)
    end
    you.mutate("good", "mutation test", false)
end

-- simulate drinking `iterations` mutation potions in a row, a bunch of times
-- (determined by `tries`) this is mostly useful when looking at the output,
-- doesn't really do much that test_random_mutations doesn't
local function test_potion(tries, iterations)
    for i=1, tries do
        you.delete_all_mutations("mutation test")
        assert(you.how_mutated(true, true, true) == 0,
                "Clearing mutations failed, currently: " .. you.mutation_overview())
        for j=1, iterations do
            simulate_mutation_pot()
            -- crawl.stderr("iter " .. j .. ", " ..
            --  you.how_mutated(true, true, true) ..
            --  " mutations. result: " .. you.mutation_overview() .. eol)
        end
        if not silent then
            crawl.stderr("Potion test try " .. i .. ", " ..
                you.how_mutated(true, true, true) .. " mutations. result: " ..
                you.mutation_overview() .. eol)
        end
    end
end

-- relies largely an asserts in the mutation code.
local function test_random_mutations(tries, iterations, chance_temporary, chance_clear)
    for i=1, tries do
        you.delete_all_mutations("mutation test")
        assert(you.how_mutated(true, true, true) == 0,
                "Clearing mutations failed, currently: " .. you.mutation_overview())
        for j=1, iterations do
            if crawl.x_chance_in_y(chance_clear, 100) then
                if crawl.coinflip() then
                    you.delete_temp_mutations(true, "Mutation test")
                    assert(you.temp_mutations() == 0,
                        "Clearing temporary mutations failed, currently at " ..
                        you.temp_mutations() .. ": " .. you.mutation_overview())
                else
                    you.delete_temp_mutations(false, "Mutation test")
                end
            end
            give_random_mutation(chance_temporary)
            -- crawl.stderr("iter " .. j .. ", " ..
            --  you.how_mutated(true, true, true) ..
            --  " mutations. result: " .. you.mutation_overview() .. eol)
        end
        if not silent then
            crawl.stderr("Random test try " .. i .. ", " ..
                you.how_mutated(true, true, true) .. " mutations. result: " ..
                you.mutation_overview() .. eol)
        end
    end
end

test_basic_mutation_stuff()
try_all_mutation_categories()
test_potion(5, mut_iterations)
test_random_mutations(tries, mut_iterations, chance_temporary, chance_clear)
you.delete_all_mutations("Mutation test")
