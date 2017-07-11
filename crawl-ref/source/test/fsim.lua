
local eol = string.char(13)

local function fsim_test()
        return wiz.quick_fsim("stone giant", 5000)
end

local function fsim_setup()
        you.init("mifi", "morningstar")
        you.set_xl(20)
        debug.flush_map_memory()
        debug.goto_place("D:1")
        debug.generate_level()
        dgn.grid(2, 2, "floor")
        dgn.grid(2, 3, "floor")
        you.moveto(2, 2)
end

local function fsim_cleanup()
        you.set_xl(1)
end

if you.wizard then
        fsim_setup()
        for i = 1,10 do
                result = fsim_test()
                crawl.stderr("AvEffDam is " .. result .. eol)
        end
        fsim_cleanup()
end
