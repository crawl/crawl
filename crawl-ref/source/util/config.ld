-- Settings for ldoc for documenting the API
add_language_extension("cc", "c")

-- LDoc is insane and demands that config.ld be in the root of the lua to be
-- documented
-- Also the order matters here for the table of contents order.
file = {
"../clua.cc",
"../dat/dlua/userbase.lua",
"../l-colour.cc",
"../l-crawl.cc",
"../l-debug.cc",
"../l-dgn.cc",
"../l-dgnbld.cc",
"../l-dgnevt.cc",
"../l-dgngrd.cc",
"../l-dgnit.cc",
"../l-dgnlvl.cc",
"../l-dgnmon.cc",
"../l-dgntil.cc",
"../l-feat.cc",
"../l-file.cc",
"../l-food.cc",
"../l-global.cc",
"../l-item.cc",
"../dat/dlua/iter.lua",
"../l-los.cc",
"../dat/dlua/macro.lua",
"../l-mapgrd.cc",
"../l-mapmrk.cc",
"../l-mons.cc",
"../l-moninf.cc",
"../l-option.cc",
"../l-spells.cc",
"../l-subvault.cc",
"../l-travel.cc",
"../l-view.cc",
"../dat/dlua/util.lua",
"../l-wiz.cc",
"../l-you.cc",
"../dat/dlua/init.lua",
}
title = "Crawl Lua API"
project = "crawllua"
sort = true
no_lua_ref = true
merge = true
format = "markdown"
wrap = true
