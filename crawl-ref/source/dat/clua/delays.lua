---------------------------------------------------------------------------
-- delays.lua:
-- Controls delayed actions' stop conditions.
--
-- What it does:
--
--  * Any message in runrest_ignore_message will *not* stop your run.
--  * Poison damage will be ignored if it is less than x% of your current
--    hp and y% of your max hp if you have defined delay_safe_poison = x:y
--  * Any monster in runrest_ignore_monster will *not* stop your run
--    if it's at least the specified distance away.
--    You can specify this with runrest_ignore_monster = regex:distance.
---------------------------------------------------------------------------

chk_interrupt_activities = function (iname, cause, extra)
    if not delay_check_params() then
        return false
    end

    if iname == 'hp_loss' then
        return delay_handle_hploss(cause, extra)
    end

    return false
end

g_rr_ignored = { }

chk_interrupt_activity.run = function (iname, cause, extra)
    if iname == 'message' then
        return rr_handle_message(cause, extra)
    end

    return false
end

-- run no longer automatically implies rest as of 0.1.3.
chk_interrupt_activity.rest = chk_interrupt_activity.run
chk_interrupt_activity.travel = chk_interrupt_activity.run

function rr_handle_message(cause, extra)
    local ch, mess = rr_split_channel(cause)
    for _, x in ipairs(g_rr_ignored) do
        if x.filter:matches(mess, ch) then
            return x.value
        end
    end
    return false
end

function rr_split_channel(s)
    local chi = string.find(s, ':')
    local channel = -1
    if chi and chi > 1 then
        local chstr = string.sub(s, 1, chi - 1)
        channel = crawl.msgch_num(chstr) -- may return nil on error
    end

    local sor = s
    if chi then
        s = string.sub(s, chi + 1, -1)
    end

    return channel, s
end

function delay_handle_hploss(hplost, source)
    -- source == 1 for poisoning
    if not g_delay_pois_curhp_ratio or not g_delay_pois_maxhp_ratio or source ~= 1 then
        return false
    end

    -- If the the poison in your system is less than the given percentages
    -- of both current and maximum hp, ignore
    local hp, mhp = you.hp()
    local poison_damage_prediction = hp - you.poison_survival()
    if (poison_damage_prediction * 100 / hp) <= g_delay_pois_curhp_ratio
       and (poison_damage_prediction * 100 / mhp) <= g_delay_pois_maxhp_ratio then
        return nil
    end

    return false
end

function delay_check_params()
    if (not g_delay_pois_maxhp_ratio or not g_delay_pois_curhp_ratio)
            and (options.delay_safe_poison or options.runrest_safe_poison)
    then
        local opt = options.delay_safe_poison or options.runrest_safe_poison
        local cur_r, max_r
        _, _, cur_r, max_r = string.find(opt, "(%d+)%s*:%s*(%d+)")
        if cur_r and max_r then
            g_delay_pois_curhp_ratio = tonumber(cur_r)
            g_delay_pois_maxhp_ratio = tonumber(max_r)
        end
    end
    return true
end

function rr_add_message(s, v, mode)
    local channel, str = rr_split_channel(s)
    if channel == nil then
        local chi = string.find(s, ':')
        if chi < 1 then
            -- I think this shouldn't be reachable? channel should be -1 if
            -- unset
            crawl.mpr("Missing channel name for runrest: '" .. s .. "'", "error")
        else
            local chstr = string.sub(s, 1, chi - 1)
            crawl.mpr("Bad channel name '" .. chstr .. "' for runrest: '" .. s .. "'", "error")
        end
        return -- skip entirely
    end
    local filter = crawl.message_filter(str, channel)
    if mode < 0 then
        for k, p in pairs (g_rr_ignored) do
            if p.filter:equals(filter) and p.value == v then
                table.remove(g_rr_ignored, k)
                return
            end
        end
    else
        local entry = { filter = filter, value = v }
        local position = table.getn(g_rr_ignored) + 1
        if mode > 1 then
            position = 1
        end
        table.insert(g_rr_ignored, position, entry)
    end
end

function rr_message_adder(v)
    local function rr_add_messages(key, value, mode)
        if mode == 0 then
            -- Clear list
            for k in pairs (g_rr_ignored) do
                g_rr_ignored[k] = nil
            end
            if value == "" then
                return
            end
        end

        local segs = crawl.split(value, ',', mode > 1)
        for _, s in ipairs(segs) do
            rr_add_message(s, v, mode)
        end
    end
    return rr_add_messages
end

chk_lua_option.runrest_ignore_message = rr_message_adder(nil)
chk_lua_option.runrest_stop_message = rr_message_adder(true)
-- alias for compatibility with obsolete travel_stop_message
chk_lua_option.travel_stop_message = rr_message_adder(true)
-----------------------------------------------------------------------

g_rr_monsters        = { {}, {} }
g_rr_monsters_moving = { {}, {} }

function rr_add_monster(mons_table, s, mode)
    local parts = crawl.split(s, ":")

    if #parts ~= 2 then
        return
    end

    local re_str = parts[1]
    local regexp = crawl.regex(re_str)
    local dist   = tonumber(parts[2])

    if dist == 0 then
        return
    end

    if mode < 0 then
        for k, r in pairs(mons_table[1]) do
            if r:equals(regexp) and mons_table[2][k] == dist then
                table.remove(mons_table[1], k)
                table.remove(mons_table[2], k)
                return
            end
        end
    else
        local position = table.getn(mons_table[1]) + 1
        if mode > 1 then
            position = 1
        end
        table.insert(mons_table[1], position, regexp)
        table.insert(mons_table[2], position, dist)
    end
end

function rr_add_monsters(key, value, mode)
    local mons_table

    if (key == "runrest_ignore_monster") then
        mons_table = g_rr_monsters
    elseif (key == "runrest_ignore_monster_moving") then
        mons_table = g_rr_monsters_moving
    else
        return
    end

    if mode == 0 then
        -- Clear list
        for k in pairs (mons_table[1]) do
            mons_table[1][k] = nil
            mons_table[2][k] = nil
        end
        if value == "" then
            return
        end
    end

    local segs = crawl.split(value, ',', mode > 1)
    for _, s in ipairs(segs) do
        rr_add_monster(mons_table, s, mode)
    end
end

function ch_mon_is_safe(mon_name, default_is_safe, moving, dist)
    if default_is_safe then
        return true
    end

    local mons_table

    -- If player is moving and the monster is in g_rr_monsters_moving,
    -- then we do the distance comparison without decreasing the
    -- distance value.
    if moving then
        mons_table  = g_rr_monsters_moving

        for i = 1, #mons_table[1] do
            local m        = mons_table[1][i]
            local min_dist = mons_table[2][i]

            if m:matches(mon_name) then
                return min_dist <= dist
            end
        end
    end

    mons_table = g_rr_monsters

    -- Reduce distance by 1 if moving, since the safety check is
    -- done *before* moving closer to the monster
    if moving then
        dist = dist - 1
    end

    for i = 1, #mons_table[1] do
        local m        = mons_table[1][i]
        local min_dist = mons_table[2][i]

        if m:matches(mon_name) then
            return min_dist <= dist
        end
    end

    return false
end

chk_lua_option.runrest_ignore_monster            = rr_add_monsters
chk_lua_option.runrest_ignore_monster_moving     = rr_add_monsters
