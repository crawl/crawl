------------------------------------------------------------------
-- kills.lua:
-- Generates fancier vanquished creatures lists.
--
-- List ideas courtesy Jukka Kuusisto and Erik Piper.
--
-- This is what implements dump_kills_detailed.
--
-- If you want to alter this, please copy it somewhere and add this line to
-- your init.txt:
--   lua_file = kills.lua
--
-- Take a look at the c_kill_list function - that's where to start if you
-- want to customize things.
------------------------------------------------------------------

function kill_filter(a, condition)
    local t = { }
    for i, k in ipairs(a) do
        if condition(k) then
            table.insert(t, k)
        end
    end
    return t
end

function show_sorted_list(list, baseindent, sortfn)
    baseindent = baseindent or "    "
    sortfn = sortfn or
               function (x, y)
                   return kills.exp(x) > kills.exp(y) or
                   (kills.exp(x) == kills.exp(y) and
                        kills.base_name(x) < kills.base_name(y))
               end
    table.sort(list, sortfn)
    for i, k in ipairs(list) do
        kills.rawwrite(baseindent .. "  " .. kills.desc(k))
    end
    kills.rawwrite(baseindent .. kills.summary(list))
end

function group_kills(a, namemap, keys, selector)
    local count = 0
    for i, key in ipairs(keys) do
        local selected = kill_filter(a,
            function (k)
                return selector(key, k)
            end
        )
        if table.getn(selected) > 0 then
            if count > 0 then
                kills.rawwrite("")
            end
            count = count + 1
            kills.rawwrite("    " .. namemap[key])
            show_sorted_list(selected)
        end
    end
end

function holiness_list(a)
    local holies = {
        strange = "Strange Monsters",
        unique  = "Uniques",
        holy    = "Holy Monsters",
        natural = "Natural Monsters",
        undead  = "Undead Monsters",
        demonic = "Demonic Monsters",
        nonliving = "Non-Living Monsters",
        plant   = "Plants",
    }
    local holysort = { "strange", "unique",
                       "natural", "nonliving",
                       "undead", "demonic", "plant" }
    kills.rawwrite("  Monster Nature")
    group_kills(a, holies, holysort,
        function (key, kill)
            return (kills.holiness(kill) == key and not kills.isunique(kill))
                or
                   (key == "unique" and kills.isunique(kill))
        end
    )
    kills.rawwrite("  " .. kills.summary(a))
end

function count_list(a, ascending)
    kills.rawwrite("  Ascending Order")
    show_sorted_list(a,
        "  ",
        function (x, y)
            if ascending then
                return kills.nkills(x) < kills.nkills(y)
                    or (kills.nkills(x) == kills.nkills(y) and
                        kills.exp(x) > kills.exp(y))
            else
                return kills.nkill(x) > kills.nkills(y)
                    or (kills.nkills(x) == kills.nkills(y) and
                        kills.exp(x) > kills.exp(y))
            end
        end
    )
end

function symbol_list(a)
    -- Create a list of monster characters and sort it
    local allsyms = { }
    for i, k in ipairs(a) do
        local ksym = kills.symbol(k)
        allsyms[ kills.symbol(k) ] = true
    end

    local sortedsyms = { }
    for sym, dud in pairs(allsyms) do table.insert(sortedsyms, sym) end
    table.sort(sortedsyms)

    kills.rawwrite("  Monster Symbol")
    for i, sym in ipairs(sortedsyms) do
        if i > 1 then
            kills.rawwrite("")
        end

        local symlist = kill_filter(a,
                function (k)
                    return kills.symbol(k) == sym
                end
        )
        kills.rawwrite("    All '" .. sym .. "'")
        show_sorted_list(symlist)
    end

    kills.rawwrite("  " .. kills.summary(a))
end

function classic_list(title, a, suffix)
    if table.getn(a) == 0 then return end
    kills.rawwrite(title)
    for i, k in ipairs(a) do
        kills.rawwrite("    " .. kills.desc(k))
    end
    kills.rawwrite("  " .. kills.summary(a))
    if suffix then
        kills.rawwrite(suffix)
    end
end

function separator()
    kills.rawwrite("----------------------------------------\n")
end

function newline()
    kills.rawwrite("")
end

-----------------------------------------------------------------------------
-- This is the function that Crawl calls when it wants to dump the kill list
-- The parameter "a" is the list (Lua table) of kills, initially sorted in
-- descending order of experience. Kill entries must be inspected using
-- kills.foo(ke).
--
-- NOTE: Comment out the kill lists that you don't like!
--
-- As of 20060224, the kill list is divided into three:
--  * Direct player kills
--  * Monsters killed by friendlies
--  * Monsters killed by other things (traps, etc.)
--
-- For each category, the game calls c_kill_list, with a table of killed
-- monsters, and the killer (who). The killer will be nil for direct player
-- kills, and a string ("collateral kills", "others") otherwise.
--
-- c_kill_list will not be called for a category if the category contains no
-- kills.
--
-- After all categories, c_kill_list will be called with no arguments to
-- indicate the end of the kill listing.

function c_kill_list(a, who, needsep)
    if not DUMP_KILL_BREAKDOWNS then
        return false
    end
    -- If there aren't any kills yet, bail out
    if not a or table.getn(a) == 0 then
        if not a and who and who ~= "" then
            -- This is the grand total
            separator()
            kills.rawwrite(who)
        end
        return true
    end

    if needsep then
        separator()
    end

    local title = "Vanquished Creatures"
    if who then
        title = title .. " (" .. who .. ")"
    end

    kills.rawwrite(title)

    -- Group kills by monster symbol
    symbol_list(a)
    newline()

    -- Group by holiness
    holiness_list(a)
    newline()

    -- Sort by kill count (ascending).
    count_list(a, true)
    newline()

    -- Classic list without zombies and skeletons
    -- Commented-out - this is a boring list.
    --[[
    -- First re-sort into descending order of exp, since
    -- count_list has re-sorted the array.
    table.sort(a, function (x, y)
                      return kills.exp(x) > kills.exp(y)
                  end);

    -- Filter out zombies and skeletons and display the rest
    classic_list(
        "  Kills minus zombies and skeletons",
        kill_filter(a,
            function (k)
                return kills.modifier(k) ~= "zombie" and
                       kills.modifier(k) ~= "skeleton"
            end
        ))

    separator()
    --]]

    -- Show only monsters with 3 digit kill count
    classic_list(
        "  The 3-digit Club",
        kill_filter(a,
            function (k)
                return kills.nkills(k) > 99
            end
        ),
        "")

    return true
end

DUMP_KILL_BREAKDOWNS = false
local function set_dump_kill_breakdowns(key, value, mode)
  DUMP_KILL_BREAKDOWNS = string.lower(value) ~= "false"
end
chk_lua_option.dump_kill_breakdowns = set_dump_kill_breakdowns
