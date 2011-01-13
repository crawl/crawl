----------------------------------------------------------------------
-- tutorial.lua:
--
-- Code for tutorials.
----------------------------------------------------------------------

function tutorial_messenger (data, triggerable, triggerer, marker, ev)

  crawl.mesclr(true)
  crawl.mpr(data.text, data.channel)
  if data.more == true then
    crawl.redraw_view()
    crawl.more()
  end
end

function tutorial_message (text, more)
  if more == nil then
    more = false
  else
    more = true
  end

  local data = {text=text, channel="tutorial", more=more}

  return function_at_spot('tutorial_messenger', data, true)
end

function tutorial_get_cmd (command)
  return "<white>" .. crawl.get_command(command) .. "</white>"
end

function tutorial_intro (msg)
  crawl.mesclr(true)
  crawl.mpr(msg, "tutorial")
  local text = "You can reread all messages at any time with "
               .. tutorial_get_cmd("CMD_REPLAY_MESSAGES") .. ".\n"
               .. "Also, press <white>Space</white> to clear the <cyan>--more--</cyan> prompts."
  crawl.mpr(text, "tutorial")
  crawl.more()
end
