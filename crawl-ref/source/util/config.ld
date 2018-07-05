-- Settings for ldoc for documenting the API
add_language_extension("cc", "c")

-- LDoc is insane and demands that config.ld be in the root of the lua to be
-- documented
file = {
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
"../l-los.cc",
"../l-mapgrd.cc",
"../l-mapmrk.cc",
"../l-moninf.cc",
"../l-mons.cc",
"../l-option.cc",
"../l-spells.cc",
"../l-subvault.cc",
"../l-travel.cc",
"../l-view.cc",
"../l-wiz.cc",
"../l-you.cc",
}
title = "Crawl Lua API"
project = "clua and dlua"
sort = true
no_lua_ref = true
merge = true
format = "markdown"
wrap = true
