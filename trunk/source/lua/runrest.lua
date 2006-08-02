---------------------------------------------------------------------------
-- runrest.lua: (requires base.lua)
-- Controls shift-running and resting stop conditions.
--
-- To use this, add this line to your init.txt:
--   lua_file = lua/runrest.lua
--
-- What it does:
-- 
--  * Any message in runrest_ignore_message will *not* stop your run.
--  * Poison damage of x will be ignored if you have at least y hp if you've
--    defined a runrest_ignore_poison = x:y option.
--    
-- IMPORTANT: You must define runrest_ options *after* sourcing runrest.lua.
---------------------------------------------------------------------------

g_rr_ignored = { }

chk_interrupt_activity.run = function (iname, cause, extra)
    if not rr_check_params() then
        return false
    end

    if iname == 'message' then
        return rr_handle_message(cause, extra)
    end

    if iname == 'hp_loss' then
        return rr_handle_hploss(cause, extra)
    end

    return false
end

function rr_handle_message(cause, extra)
    local ch, mess = rr_split_channel(cause)
    for _, m in ipairs(g_rr_ignored) do
        if m:matches(mess, ch) then
            return nil
        end
    end
    return false
end

function rr_split_channel(s)
    local chi = string.find(s, ':')
    local channel = -1
    if chi and chi > 1 then
        local chstr = string.sub(s, 1, chi - 1)
        channel = crawl.msgch_num(chstr)
    end

    local sor = s
    if chi then
        s = string.sub(s, chi + 1, -1)
    end

    return channel, s
end

function rr_handle_hploss(hplost, source)
    -- source == 1 for poisoning
    if not g_rr_yhpmin or not g_rr_hplmax or source ~= 1 then
        return false
    end

    -- If the hp lost is smaller than configured, and you have more than the
    -- minimum health, ignore this poison event.
    if hplost <= g_rr_hplmax and you.hp() >= g_rr_yhpmin then
        return nil
    end
    
    return false
end

function rr_check_params()
    if g_rrim ~= options.runrest_ignore_message then
        g_rrim = options.runrest_ignore_message
        rr_add_messages(nil, g_rrim)
    end

    if ( not g_rr_hplmax or not g_rr_yhpmin ) 
            and options.runrest_ignore_poison
    then
        local opt = options.runrest_ignore_poison
        local hpl, hpm
        _, _, hpl, hpm = string.find(opt, "(%d+)%s*:%s*(%d+)")
        if hpl and hpm then
            g_rr_hplmax = tonumber(hpl)
            g_rr_yhpmin = tonumber(hpm)
        end
    end
    return true
end

function rr_add_message(s)
    local channel, str = rr_split_channel(s)
    table.insert( g_rr_ignored, crawl.message_filter( str, channel ) )
end

function rr_add_messages(key, value)
    local segs = crawl.split(value, ',')
    for _, s in ipairs(segs) do
        rr_add_message(s)
    end
end

chk_lua_option.runrest_ignore_message = rr_add_messages
