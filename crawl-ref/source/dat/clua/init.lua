------------------------------------------------------------------------------
-- init.lua
-- Common CLua initializtion.
------------------------------------------------------------------------------

-----
-- Set up echoing function for interpreter.
-----

function echoall(...)
  util.foreach({...}, crawl.mpr)
end

__echo = echoall
