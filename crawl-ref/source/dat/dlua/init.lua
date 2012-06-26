------------------------------------------------------------------------------
-- init.lua
-- Common CLua initializtion.
------------------------------------------------------------------------------

-----
-- Set up echoing function for interpreter.
-----

function echoall(...)
  util.foreach({...}, function(x)
      crawl.mpr(tostring(x))
    end)
end

__echo = echoall

-- A wrapper for the low-level sequence of commands needed to produce
-- message output.
function crawl.message(message, channel)
  crawl.mpr(message, channel)
  crawl.flush_prev_message()
  crawl.delay(0)
end
