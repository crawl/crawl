-- A skill training simulator
-- To simulate the skill progression of a Minotaur Fighter starting with a
-- hand axe, run it with:
-- crawl -script training_simu MiFi handaxe 2> results.csv

local function print_skills()
    local line = you.xl()
    for sk, foo in pairs(skills) do
        line = string.format("%s,%d.%02d", line, you.skill(sk), you.skill_progress(sk))
    end
    line = line .. "," .. you.skill_cost_level()
    crawl.stderr(line)
end

local args = script.simple_args()
if #args == 0 then
  script.usage("Usage: training_simu <combo> [weapon]")
end
local job = string.sub(args[1], 3)

-- Init player class
local weapon_skill = you.init(args[1], args[2])
crawl.stderr(you.race() .. " " .. you.class())

-- Numbers for each skill are: period, start (XL), stop (skill level)
if (job == "Fi") then
    skills = { Fighting          = {  2, 1, 27 },
               [weapon_skill]    = {  1, 1, 18 },
               Armour            = {  3, 1, 27 },
               Shields           = {  3, 1, 27 },
               Traps             = { 10, 1,  8 },
               Evocations        = {  5, 5, 10 },
              }
elseif (job == "FE") then
    skills = { Dodging           = {  3, 1, 27 },
               Stealth           = {  4, 1,  6 },
               Shields           = {  5, 8,  5 },
               Traps             = { 10, 1,  8 },

               Spellcasting      = {  2, 1, 27 },
               Conjurations      = {  1, 1, 27 },
               ["Fire Magic"]    = {  1, 1, 27 },
               ["Air Magic"]     = {  1, 13, 6 },
               Evocations        = {  5, 5,  5 },
              }
else
    crawl.stderr("Background " .. job .. " not supported.")
    return
end

-- Print title line
local line = "XL"
for sk, foo in pairs(skills) do
    line = line .. "," .. sk
end
line = line .. ",s_c_l"
crawl.stderr(line)

-- Print level 1 skills
print_skills()

local i = 0
for level = 2, 27 do

    -- We skip the stat prompt
    if (you.xl() + 1) % 3 == 0 then
        crawl.sendkeys('s')
    end

    -- gain a level
    local exp = you.exp_needed(level) - you.exp()
    you.gain_exp(exp)

    -- we empty the XP pool into the skills
    while you.exp_pool() > 0 do
        local next_level = false
        for sk, t in pairs(skills) do
            local full_exercise = 10
            if you.skill(sk) == 0 then full_exercise = 5 end
            if i % t[1] == 0 and you.xl() >= t[2] and you.skill(sk) < t[3] then
                if you.exercise(sk) < full_exercise then next_level = true end
            end
        end
        i = i + 1
        if next_level then break end
    end

    print_skills()
end

line = "skill_points"
for sk, foo in pairs(skills) do
    line = line .. "," .. you.skill_points(sk)
end
crawl.stderr(line)
