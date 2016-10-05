-- a crude constriction escape simulator

local niters = 10000
local mons = {  [1]="ball python",
                [5]="naga",
                [10]="naga_warrior",
                [15]="nagaraja",
                [23]="tentacled_monstrosity"
            }

for hd,mon in pairs(mons) do
    crawl.stderr(string.format("Against %s HD %d", mon, hd))
    crawl.stderr("| str |   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |")
    for str = 3, 27, 3 do
        local line = string.format("|  %02d |", str)
        for turns = 1, 8 do
            local escape = 0
            for i = 1, niters do
                if crawl.roll_dice(4+turns, 8 + crawl.div_rand_round(str, 4))
                   >= crawl.roll_dice(5, 8 + crawl.div_rand_round(hd, 4)) then
                    escape = escape + 1
                end
            end
            line = line .. string.format(" %5.1f |", escape * 100 / niters)
        end
        crawl.stderr(line)
    end
    crawl.stderr("")
end
