---------------------------------------------------------------------------
-- userbase.lua:
-- Base Lua definitions that other Lua scripts rely on (auto-loaded).
---------------------------------------------------------------------------

-- Lua global data
chk_interrupt_macro     = { }
chk_interrupt_activity  = { }
chk_lua_option          = { }

-- Push into this table, rather than indexing into it.
chk_lua_save            = { }
chk_force_autopickup    = { }

-- Data placed in this table will automatically persist through
-- saves and deaths. See persist.lua for the details of how this is done.
if not c_persist then
    c_persist = { }
end

function c_save()
    if not chk_lua_save then
        return
    end

    local res = ""
    for i, fn in ipairs(chk_lua_save) do
        res = res .. fn(file)
    end
    return res
end

-- This function returns true to tell Crawl not to process the option further
function c_process_lua_option(key, val, mode)
    if chk_lua_option and chk_lua_option[key] then
        return chk_lua_option[key](key, val, mode)
    end
    return false
end

function c_interrupt_macro(iname, cause, extra)
    -- If some joker undefined the table, stop all macros
    if not chk_interrupt_macro then
        return true
    end

    -- Maybe we don't know what macro is running? Kill it to be safe
    -- We also kill macros that don't add an entry to chk_interrupt_macro.
    if not c_macro_name or not chk_interrupt_macro[c_macro_name] then
        return true
    end

    return chk_interrupt_macro[c_macro_name](iname, cause, extra)
end

-- Notice that c_interrupt_activity defaults to *false* whereas
-- c_interrupt_macro defaults to *true*. This is because "false" really just
-- means "go ahead and use the default logic to kill this activity" here,
-- whereas false is interpreted as "no, don't stop this macro" for
-- c_interrupt_macro.
--
-- If c_interrupt_activity, or one of the individual hooks wants to ensure that
-- the activity continues, it must return *nil* (make sure you know what you're
-- doing when you return nil!).
function c_interrupt_activity(aname, iname, cause, extra)
    -- If some joker undefined the table, bail out
    if not chk_interrupt_activity then
        return false
    end

    if chk_interrupt_activities then
        local retval = chk_interrupt_activities(iname, cause, extra)
        if retval ~= false then
            return retval
        end
    end

    -- No activity name? Bail!
    if not aname or not chk_interrupt_activity[aname] then
        return false
    end

    return chk_interrupt_activity[aname](iname, cause, extra)
end

function opt_boolean(optname, default)
    default = default or false
    local optval = options[optname]
    if optval == nil then
        return default
    else
        return optval == "true" or optval == "yes"
    end
end

function ch_force_autopickup(it, name)
    if not chk_force_autopickup then
        return
    end
    for i = 1, #chk_force_autopickup do
        res = chk_force_autopickup[i](it, name)
        if type(res) == "boolean" then
            return res
        end
    end
end

function add_autopickup_func(func)
    table.insert(chk_force_autopickup, func)
end

function clear_autopickup_funcs()
    for i in pairs(chk_force_autopickup) do
        chk_force_autopickup[i] = nil
    end
end
