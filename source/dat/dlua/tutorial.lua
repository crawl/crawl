----------------------------------------------------------------------
-- tutorial.lua:
--
-- Code for tutorials.
----------------------------------------------------------------------

function tutorial_messenger (data, triggerable, triggerer, marker, ev)

  crawl.clear_messages(true)
  crawl.mpr(data.text, data.channel)
  if data.onetime == true then
    triggerable:remove(marker)
  end
end

function tutorial_message (text, onetime)
  -- defaults to false
  if onetime == nil then
    onetime = false
  end

  local data = {text=text, channel="tutorial", onetime=onetime}

  return function_at_spot('tutorial_messenger', data, true)
end

function tutorial_intro (msg)
  if msg ~= nil then
    crawl.clear_messages(true)
    crawl.mpr(msg, "tutorial")
  end
  crawl.tutorial_msg("tutorial intro")
  crawl.more()
end

function tutorial_messenger_db(data, triggerable, triggerer, marker, ev)

  crawl.clear_messages(true)
  crawl.tutorial_msg(data.text, data.exit)
  if data.onetime == true then
    triggerable:remove(marker)
  end
end

function tutorial_msg(text, onetime, exit)
  -- defaults to false
  if onetime == nil then
    onetime = false
  end

  if exit == nil then
    exit = false
  end

  local data = {text=text, onetime=onetime, exit=exit}

  return function_at_spot('tutorial_messenger_db', data, true)
end
