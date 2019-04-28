-- This test prints out some random numbers from seed 1. If they ever change,
-- something has broken the stability of the rng.

local eol = string.char(13)

function big_rng_test()
    crawl.stderr("Running big rng test, this may take a while...")
    for i = 1, 0xffffffff do
        crawl.random2(i)
    end
    debug.reset_rng(1)

end

local results = {51, 11, 78, 21, 19, 95, 21, 43, 33, 75}

debug.reset_rng(1)
crawl.stderr("Here are 10 random numbers from seed 1:" .. eol)
for i = 1,10 do
    n = crawl.random2(100)
    crawl.stderr("    " .. n)
    assert(n == results[i])
end
crawl.stderr(eol)

debug.reset_rng(1)

--big_rng_test()
