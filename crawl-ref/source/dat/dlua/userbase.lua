--- Entry points crawl uses for calling into lua.
--
-- Crawl contacts clua through hooks. Hooks can be interacted with either by
-- altering a hook table, defining certian functions, or the interface
-- functions described here.
--
-- *Note:* This is not a real module. All names described here are in the
-- global clua namespace.
-- @module Hooks

--- Macro interrupt table.
-- Maps macro names to interrupt handling functions.
-- When crawl needs to interrupt a macro it calls this function with the
-- parameters name, cause, and extra. The function returns true if the macro
-- was interrupted
-- @table chk_interrupt_macro
chk_interrupt_macro     = { }
--- Activity interrupt table.
-- Maps activity names to interrupt functions
-- When crawl needs to interrupt an internal delay it looks up the delay by
-- name in this table and if a function is present, calls it with the
-- parameters name, cause, and extra. The function returns true to interrupt,
-- false to express no opinion, and nil for "don't interrupt". Return nil only
-- if you really know what you're doing.
-- @table chk_interrupt_activity
chk_interrupt_activity  = { }

--- Lua option extension table
-- Add lua processed options to options.txt.
-- Keys are treated as option names.
-- Value should be a function that takes parameters key, value, and mode.
-- When processing an option defined in this way crawl will call the
-- corresponding function with value the string that is on the other side of
-- the equals sign and one of the following mode values describing the
-- assignment type:
--
-- - 0 for `=`
-- - 1 for `+=`
-- - -1 for `-=`
-- - 2 for `^=`
--
-- The global table @{Globals.options} is provided as a target store for these
-- options.
-- @see opt_boolean
-- @table chk_lua_option
chk_lua_option          = { }

--- Save hooks.
-- Push into this table, rather than indexing into it.
-- A list of functions which get called when saving. They are expected to
-- return strings of lua that will be executed on load. Data saved with this
-- method is associated with the character save and will be lost on death.
-- For permanet storage see @{c_persist}.
-- @table chk_lua_save
chk_lua_save            = { }

--- Autopickup functions
-- User interface via @{add_autopickup_func} and @{clear_autopickup_funcs}.
-- @local
-- @table chk_force_autopickup
chk_force_autopickup    = { }

--- Persistent data store.
-- Data placed in this table will automatically persist through saves and
-- deaths. See `persist.lua` for the internal details of how this is done.
-- @table c_persist
if not c_persist then
    c_persist = { }
end

--- Save hook function.
-- Walks the chk_lua_save table.
-- @local
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

--- Internal Lua option processing hook.
-- This function returns true to tell Crawl not to process the option further.
-- Mode is described in @{chk_lua_option}.
-- @param key
-- @param val
-- @param mode
-- @local
function c_process_lua_option(key, val, mode)
    if chk_lua_option and chk_lua_option[key] then
        return chk_lua_option[key](key, val, mode)
    end
    return false
end

--- Internal macro interrupt hook.
-- Entry point for chk_interrupt_macro
-- @local
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

--- Internal activity interrupt hook.
-- Entry point for chk_interrupt_activity
--
-- Notice that c_interrupt_activity defaults to *false* whereas
-- c_interrupt_macro defaults to *true*. This is because "false" really just
-- means "go ahead and use the default logic to kill this activity" here,
-- whereas false is interpreted as "no, don't stop this macro" for
-- c_interrupt_macro.
--
-- If c_interrupt_activity, or one of the individual hooks wants to ensure that
-- the activity continues, it must return *nil* (make sure you know what you're
-- doing when you return nil!).
-- @local
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

--- Define a boolean option.
-- Convenience function for use with @{chk_lua_option}.
-- @usage chk_lua_option["myboolopt"] = opt_boolean
function opt_boolean(optname, default)
    default = default or false
    local optval = options[optname]
    if optval == nil then
        return default
    else
        return optval == "true" or optval == "yes"
    end
end

--- Internal entrypoint to autopickup functions
-- @param it items.Item
-- @param name
-- @local
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

--- Add an autopickup function.
-- Autopickup functions are passed an items.Item and an object name, and are
-- expected to return true for "yes pickup", false for "no do not". Any other
-- return means "no opinion".
-- @param func autopickup function
function add_autopickup_func(func)
    table.insert(chk_force_autopickup, func)
end

--- Clear the autopickup function table.
function clear_autopickup_funcs()
    for i in pairs(chk_force_autopickup) do
        chk_force_autopickup[i] = nil
    end
end

-- The remainder of these hooks are called elsewhere in the codebase from
-- various places. We document the user-intended ones here.

--- Do we want to target this monster?
--
-- This hook can be re-defined to alter the auto-targeter.
--
-- Called by the targeter by each cell in sight, spiralling outward from
-- the player, until a target is found, to set the default target
-- Uses player centered coordinates.
--
-- Return true for yes, false for no, and nil for no opinion.
-- @tparam int x
-- @tparam int y
-- @treturn boolean|nil
-- @function ch_target_monster

--- Do we want to try to shadow step here?
--
-- This hook can be re-defined to alter the auto-targeter.
--
-- Called by the targeter by each cell in sight, spiralling outward from
-- the player, until a target is found, to set the default target
-- Uses player centered coordinates. If this function has no
-- opinion @{ch_target_monster} is called in fall-through.
--
-- Return true for yes, false for no, and nil for no opinion.
-- @tparam int x
-- @tparam int y
-- @treturn boolean|nil
-- @function ch_target_shadow_step

--- Do we want to target this monster for an explosion?
--
-- This hook can be re-defined to alter the auto-targeter.
--
-- Called by the explosion targeter by each cell in sight, spiralling outward
-- from the player, until a target is found, to set the default target Uses
-- player centered coordinates.
--
-- Return true for yes, false for no, and nil for no opinion.
-- @tparam int x
-- @tparam int y
-- @treturn boolean|nil
-- @function ch_target_monster_expl

--- What letter should this item get?
--
-- This hook can be re-defined to provide detailed customization.
-- It will be ignored if it fails to return a free slot.
--
-- @tparam items.Item the item being put into inventory
-- @treturn int free slot index to put the item in
-- @see items
-- @function c_assign_invletter

--- Start of turn hook.
--
-- This hook can be defined to provide start of turn checks.
--
-- Crawl calls this function at the start of each turn if there are no
-- remaining command repeats, macros, delays, or inputs in the buffer. This is
-- done before reading new input.
--
-- @function ready

--- Is this monster safe?
--
-- This hook can be defined to add extra safety checks or overrides.
-- It is called when crawl wants to consider interrupting a repeat action
-- because there are unsafe monsters around.
--
-- @tparam monster.mon-info monster info
-- @tparam boolean is_safe what crawl currently thinks about the monster
-- @tparam boolean moving is this safe to move near
-- @tparam int dist how far away is this monster
-- @treturn boolean is it safe?
-- @function ch_mon_is_safe

--- Choose a stat.
--
-- This hook can be defined to answer the stat choice prompt automatically.
--
-- Called on levelup to prompt for a stat choice.
-- Returns a string with the stat choice.
--
-- @treturn string stat choice
-- @function choose_stat_gain

--- Answer a prompt.
--
-- This hook can be defined to answer a yesno prompt automatically.
--
-- @return boolean|nil true for yes, false for no, nil for pass
-- @function c_answer_prompt

--- Automatically distribute or accept potion of experience distributions.
--
-- Called when presented with the skill menu after quaffing a potion of
-- experience. Can use the skilling functions in @{you} to change training.
--
-- @treturn boolean true for accept false to prompt user
-- @function auto_experience

--- Select skills for training.
--
-- Called when no skills are currently selected for training.
-- Can use the skilling functions in @{you} to change training.
-- Will prompt the user if this function fails to select skills for training.
--
-- @treturn boolean true to accept the skilling, false to prompt the user
-- @function skill_training_needed

--- Is this trap safe?
--
-- This hook can be defined to extend trap safety checks.
--
-- Crawl will call this hook with the trap name when the player tries to move
-- onto a tile with a trap. A failed check will result in the user being
-- prompted if they try to move onto the trap.
--
-- @tparam string trapname
-- @treturn boolean
-- @function c_trap_is_safe

--- Post runrest hook.
--
-- This hook can be defined to execute lua when some form of rest or autotravel
-- stops.
--
-- The parameter is what kind of running was just stopped, and has the
-- following possible values:
--
-- - "travel" for autotravel (with XG or similar)
-- - "interlevel" for interlevel travel
-- - "explore" for autoexplore
-- - "explore_greedy" for autoexplore + item pickup
-- - "run" for a plain run (Shift+Dir)
-- - "" for a rest
--
-- @tparam string kind
-- @function ch_stop_running

--- Pre runrest hook.
--
-- This hook can be defined to execute lua when some form of rest or autotravel
-- starts.
--
-- The parameter is what kind of running was just stopped, and has the
-- following possible values:
--
-- - "travel" for autotravel (with XG or similar)
-- - "interlevel" for interlevel travel
-- - "explore" for autoexplore
-- - "explore_greedy" for autoexplore + item pickup
-- - "run" for a plain run (Shift+Dir)
-- - "" for a rest
--
-- @tparam string kind
-- @function ch_start_running
