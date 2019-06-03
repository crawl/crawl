-- script interface to the explorer.lua package

crawl_require('dlua/explorer.lua')

local basic_usage = [[
Usage: seed_explorer.lua -seed <seed> [<seed> ...] [-depth <depth>]
    <seed>: either a number, or 'random'.
    <depth>: either a number, or 'all'. Defaults to 'all'.]]

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
    usage_error("No seeds supplied!")
end

-- let these be converted to numbers on the crawl side
local seed_seq = args["-seed"]

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

if max_depth == nil then
    max_depth = #explorer.generation_order
end

explorer.catalog_seeds(seed_seq, max_depth, explorer.available_categories)
