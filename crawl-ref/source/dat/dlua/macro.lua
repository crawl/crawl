------------------------------------------------------------------
-- @module Globals

--- Crawl macro framework for Lua.
-- Macros are called as Lua coroutines. If the macro yields false, the
-- coroutine is discarded (assuming an error). If the macro yields
-- true, Crawl will start a macro delay for the macro and call the
-- coroutine again next turn. If the macro just returns, the coroutine
-- is assumed to be done.
--
-- Why coroutines: Macros may need to perform actions that take
-- multiple turns, which requires control to return to Crawl to
-- perform world updates between actions. Coroutines are the simplest
-- way to pass control back and forth without losing the macro's
-- state.
--
-- This is the internal crawl hook for executing a macro. If you would like to
-- start one from clua use @{crawl.runmacro}.
-- @param fn macro name
-- @local
function c_macro(fn)
   if fn == nil then
      if c_macro_coroutine ~= nil then
         local coret, mret
         coret, mret = coroutine.resume(c_macro_coroutine)
         if not coret or not mret then
            c_macro_coroutine = nil
            c_macro_name      = nil
         end
         if not coret and mret then
            error(mret)
         end
         return (coret and mret)
      end
      return false
   end
   if _G[fn] == nil or type(_G[fn]) ~= 'function' then
      return false
   end
   c_macro_name = fn
   c_macro_coroutine = coroutine.create(_G[fn])
   return c_macro()
end
