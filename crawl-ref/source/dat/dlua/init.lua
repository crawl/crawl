------------------------------------------------------------------------------
-- @module crawl
------------------------------------------------------------------------------

--- Echoing function for the interpreter.
-- @local
function echoall(...)
  util.foreach({...}, function(x)
      crawl.mpr(tostring(x))
    end)
end

__echo = echoall

--- A wrapper for the low-level sequence of commands needed to produce
-- message output.
-- @tparam string message
-- @tparam int channel messge channel number
-- @see crawl.mpr, crawl.msgch_name, crawl.msgch_num
function crawl.message(message, channel)
  crawl.mpr(message, channel)
  crawl.flush_prev_message()
  crawl.delay(0)
end
