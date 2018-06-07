local silent = true -- change to false to look at the result of stress tests
local mut_iterations = 500 -- mutate each try this many times
local tries = 40

local chance_temporary = 10 -- in 100.
local chance_clear = 2 -- in 100. Chance to clear some temporary mutations. Note that some mutations clear others anyways.

local eol = string.char(13)

local function print_mutstate(prefix)
    if not silent then
        crawl.stderr(prefix ..
                you.how_mutated(true, true, true) .. " mutations. result: " ..
                you.mutation_overview() .. eol)
    end
end

-- could still add more things in here.
-- see mut_species.lua for testing of innate mutations & their interactions
local function test_basic_mutation_stuff()
    you.delete_all_mutations("mutation test")
    ------------------
    -- test some mutation interactions (see mutation.cc:conflict)
    -- test trading off (type 1)
    you.mutate("robust", "basic mutation test", false)
    you.mutate("frail", "basic mutation test", false)
    assert(you.get_base_mutation_level("robust", true, true, true) == 0)
    for i=1, 10 do
        you.mutate("robust", "basic mutation test", false)
    end
    assert(you.get_base_mutation_level("robust", true, true, true) == 3)
    you.mutate("frail", "basic mutation test", true) -- should now have robust 3, temp frail 1
    assert(you.get_base_mutation_level("robust", true, true, true) == 3)
    assert(you.get_base_mutation_level("frail", true, true, true) == 1)
    -- quick test of the lua binding while we're here since default args with booleans are a bit tricky
    assert(you.get_base_mutation_level("frail") == you.get_base_mutation_level("frail", true, true, true))
    assert(you.get_base_mutation_level("frail") == you.get_base_mutation_level("frail", false, true, false))
    assert(you.get_base_mutation_level("frail", false, true) == you.get_base_mutation_level("frail", false, true))

    -- test forced clearing for mutations that can coexist (type 0)
    assert(you.mutate("regeneration", "basic mutation test", false))
    assert(you.mutate("regeneration", "basic mutation test", false))
    assert(you.mutate("slow metabolism", "basic mutation test", false))
    assert(you.get_base_mutation_level("regeneration", true, true, true) == 0)
    assert(you.get_base_mutation_level("slow metabolism", true, true, true) == 1)
    assert(you.mutate("regeneration", "basic mutation test", false, false)) -- not forced, should end up with both?
    assert(you.get_base_mutation_level("regeneration") == 1)

    -- test forced clearing for mutations that can't coexist (type -1)
    assert(you.mutate("fire resistance", "basic mutation test", false))
    assert(you.mutate("fire resistance", "basic mutation test", false))
    assert(not you.mutate("heat vulnerability", "basic mutation test", false, false)) -- non-forced mutation should fail
    assert(you.get_base_mutation_level("fire resistance") == 2)
    assert(you.get_base_mutation_level("heat vulnerability") == 0)
    assert(you.mutate("heat vulnerability", "basic mutation test", false, true)) -- forced mutation should fully clear hooves
    assert(you.get_base_mutation_level("fire resistance") == 0)
    assert(you.get_base_mutation_level("heat vulnerability") == 1)

    -- test mutations that can simply coexist
    assert(you.mutate("fire resistance", "basic mutation test", false))
    assert(you.mutate("fire resistance", "basic mutation test", false))
    assert(you.mutate("cold resistance", "basic mutation test", false, false))
    assert(you.get_base_mutation_level("fire resistance") == 2)
    assert(you.get_base_mutation_level("cold resistance") == 1)
    assert(you.mutate("cold resistance", "basic mutation test", false, true))
    assert(you.get_base_mutation_level("fire resistance") == 2)
    assert(you.get_base_mutation_level("cold resistance") == 2)

    ------------------
    -- test some physiology conflicts interactions (see mutation.cc:physiology_mutation_conflict)
    -- this isn't exhaustive
    assert(you.mutate("icy blue scales", "basic mutation test"))
    assert(you.mutate("iridescent scales", "basic mutation test"))
    assert(you.mutate("large bone plates", "basic mutation test"))
    assert(not you.mutate("molten scales", "basic mutation test")) -- three scale limit

    assert(not you.mutate("spit poison", "basic mutation test")) -- only for nagas
    -- could add other species conditions here using you.change_species

    assert(you.mutate("talons", "basic mutation test"))
    assert(not you.mutate("hooves", "basic mutation test")) -- covered by physiology conflict

    print_mutstate("basic results: ")
    you.delete_all_mutations("mutation test")
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
local function test_potion(tries, iterations, premutate)
    sum = 0
    for i=1, tries do
        you.delete_all_mutations("mutation test")
        assert(you.how_mutated(true, true, true) == 0,
                "Clearing mutations failed, currently: " .. you.mutation_overview())
        for i=1, premutate do
            -- note: won't guarantee `premutate` mutations, because some will
            -- cancel each other out. This tops out at around 10 mutation
            -- levels by this method.
            give_random_mutation(0.0)
        end
        for j=1, iterations do
            simulate_mutation_pot()
        end
        print_mutstate("Potion test try " .. i .. ", ")
        sum = sum + you.how_mutated(true, true, true)
    end
    mean = sum / tries
    if not silent then
        crawl.stderr("Mean resulting mutations: " .. mean .. eol)
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
        end
        print_mutstate("Random test try " .. i .. ", ")
    end
end

test_basic_mutation_stuff()
try_all_mutation_categories()
test_potion(5, mut_iterations, 0)
test_random_mutations(tries, mut_iterations, chance_temporary, chance_clear)
you.delete_all_mutations("Mutation test")
