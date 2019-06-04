-- script interface to the explorer.lua package

crawl_require('dlua/explorer.lua')

-- This requires a debug build to run, as well as fake_pty, which may need to
-- be built manually. It also needs a pty-based system, i.e. linux or mac.
--   make debug    OR (this will be a lot faster):    make profile
--   make util/fake_pty
--
-- examples.
-- full catalog for a seed:
--   util/fake_pty ./crawl -script seed_explorer.lua -seed 1
-- find all artefacts in a seed:
--   util/fake_pty ./crawl -script seed_explorer.lua -seed 1 -cats monsters items -mon-items -artefacts
-- look at D:1 for 10 random seeds:
--   util/fake_pty ./crawl -script seed_explorer.lua -seed random -count 10 -depth 1
-- find all artefacts in shops (and print all shop names):
--   util/fake_pty ./crawl -script seed_explorer.lua -seed 1 -depth all -cats features items -shops -artefacts
--
-- When running scripts with fake_pty, all output goes to stderr, so to
-- redirect this to a file you will need to do something like:
--   util/fake_pty ./crawl -script seed_explorer.lua -seed 1 > out.txt 2>&1

local basic_usage = [=[
Usage: seed_explorer.lua -seed <seed> ([<seed> ...]|[-count <n>]) [-depth <depth>] [-cats <cat> [<cat ...]] [-artefacts] [-mon-items]
    <seed>:  either a number, or 'random'. Random values are 32 bits only.
    <n>:     a number of times to iterate from <seed>. If seed is a number, this
             will count up; if it is 'random' it will choose n random seeds.
             Note that this converts seed values to doubles in lua, so limits
             the range of possible values to some degree.
    <depth>: either a number, or 'all'. Defaults to 'all'.
    <cat>:   a seed explorer category, drawn from:
               {]=] ..
        table.concat(explorer.available_categories, ", ") .. [[}
             The '-cats' list defaults to all categories.
    Category-specific flags (ignored if category is not shown):
        -artefacts: show only artefact items (items, monsters).
        -mon-items: show only monsters with items (monsters).
        -shops:     show only shops (items, features).
        -all-mons:  show all monsters (monsters).
        -all-items: show all items on ground (items).]]

function parse_args(args, err_fun)
    accum_init = { }
    accum_params = { }
    cur = nil
    for _,a in ipairs(args) do
        if string.find(a, '-') == 1 then
            cur = a
            if accum_params[a] ~= nil then err_fun("Repeated argument '" .. a .."'") end
            accum_params[a] = { }
        else
            if cur == nil then
                accum_init[#accum_init + 1] = a
            else
                accum_params[cur][#accum_params[cur] + 1] = a
            end
        end
    end
    return accum_init, accum_params
end

function one_arg(args, a)
    if args[a] == nil or #(args[a]) ~= 1 then return nil end
    return args[a][1]
end

function usage_error(extra)
    local err = basic_usage
    if extra ~= nil then
        err = err .. "\n" .. extra
    end
    script.usage(err)
end

local arg_list = crawl.script_args()
local args_init, args = parse_args(arg_list, usage_error)

if #arg_list == 0 or #args_init ~= 0 or args["-seed"] == nil or #(args["-seed"]) < 1 then
    usage_error("\nNo seed(s) supplied!")
end

-- let these be converted to numbers on the crawl side
local seed_seq = args["-seed"]

local count = 1
if args["-count"] ~= nil then
    count = tonumber(one_arg(args, "-count"))
    if count == nil then usage_error("\nInvalid argument to -count") end
end

if count > 1 then
    if seed_seq[1] == "random" then
        for i = 2, count do seed_seq[#seed_seq+1] = "random" end
    else
        -- this has the caveats that come with forcing this to be a number...
        local n = tonumber(seed_seq[1])
        for i = n+1, n+count-1 do
            seed_seq[#seed_seq+1] = i
        end
    end
end

math.randomseed(crawl.millis())
for _, seed in ipairs(seed_seq) do
    if seed == "random" then
        -- intentional use of non-crawl random(). random doesn't seem to accept
        -- anything bigger than 32 bits for the range.
        seed_seq[_] = math.random(0x7FFFFFFF)
    end
end

local max_depth = nil
if args["-depth"] ~= nil then
    max_depth = one_arg(args, "-depth")
    if max_depth == "all" then
        max_depth = nil
    else
        max_depth = tonumber(max_depth)
        if max_depth == nil then
            usage_error("\n<depth> must be a number or 'all'!")
        end
    end
end

local categories = { }
if args["-cats"] == nil then
    categories = explorer.available_categories
else
    categories = args["-cats"]
end

categories = util.filter(explorer.is_category, categories)
if #categories == 0 then
    usage_error("\nNo valid categories specified!")
end

if max_depth == nil then
    max_depth = #explorer.generation_order
end

-- TODO: these are kind of ad hoc, maybe some kind of more general interface?
local arts_only = (args["-artefacts"] ~= nil)
local all_items = (args["-all-items"] ~= nil)
local shops_only = (args["-shops"] ~= nil)
if arts_only and all_items then
    usage_error("\n-artefacts and -all-items are not compatible.")
end
local mon_items_only = (args["-mon-items"] ~= nil)
local all_mons = (args["-all-mons"] ~= nil)
if mon_items_only and all_mons then
    usage_error("\n-mon-items and -all-mons are not compatible.")
end

explorer.reset_to_defaults()
if arts_only then explorer.item_notable = explorer.arts_only end
if all_items then explorer.item_notable = function (x) return true end end
if shops_only then -- TODO: generalize the technique here
    local old_fun = explorer.item_notable
    explorer.item_notable = function(i) return i.is_in_shop and old_fun(i) end
    explorer.feat_notable = function(f) return f == "enter_shop" end
end
if mon_items_only then
    explorer.mons_notable = function (m) return false end
    explorer.mons_feat_filter = function (f)
            return f:find("item:") == 1 and f or nil
        end
end
if all_mons then explorer.mons_notable = function (x) return true end end

explorer.catalog_seeds(seed_seq, max_depth, categories)
explorer.reset_to_defaults()
