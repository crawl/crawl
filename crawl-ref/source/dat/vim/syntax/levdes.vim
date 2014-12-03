" Vim syntax file
" Language:	Dungeon Crawl level design (.des) files.
" Maintainer:	Darshan Shaligram <scintilla@gmail.com>
" Last Change:	2007 Jun 28
" Remark:	Basic Vim syntax highlighting for Dungeon Crawl Stone Soup
"               level design (.des) files.
"
" How to use this:
" * Put levdes.vim (this file) under ~/.vim/syntax (or similar directory for
"   your system - usually C:\Program Files\Vim\vimfiles\syntax on Windows).
" * In your .vimrc, add this line:
"     au BufRead,BufNewFile *.des set syntax=levdes
" Thereafter, any .des files you edit in (g)vim will use syntax highlighting.

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

if !exists("main_syntax")
  let main_syntax = 'des'
endif

syn include @desLua syntax/lua.vim

syn case match

syn match desLuaBlock /\(lua\)\?\s\+{{/ contained
syn match desOtherLuaBlock /^\(prelude\|lua\|validate\|epilogue\|veto\)\?\s*{{/ contained
syn match desLuaBlockEnd /}}/ contained
"syn match desColonLine /^\s*:/ contained

syn cluster desLuaGroup contains=desLuaBlock,desOtherLuaBlock,desLuaBlockEnd

syn region desLua start=/^\s*\(lua\)\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desLuaCol start=/^\s*:/ end=/$/ contains=@desLuaGroup,@desLua keepend
syn region desVal start=/^\s*validate\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desPre start=/^\s*prelude\?\s*{{/ end=/}}\s*$/ contains=@desLuaGroup,@desLua keepend
syn region desEpi start=/^\s*epilogue\?\s*{{/ end=/}}\s*$/ contains=@desLuagroup,@deslua keepend
syn region desVet start=/^\s*veto\?\s*{{/ end=/}}\s*$/ contains=@desLuagroup,@deslua keepend

setlocal iskeyword+=:
setlocal iskeyword+=-

" These have extra matching going on, so not in desDeclarator (global)
syn keyword desDec NAME: COLOUR: SUBST: NSUBST: SHUFFLE: CLEAR: LROCKCOL: LFLOORCOL: contained

syn region desSubst start=/^SUBST:\s*/ end=/$/ contains=desDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desNsubst start=/^NSUBST:\s*/ end=/$/ contains=desDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desShuffle start=/^SHUFFLE:\s*/ end=/$/ contains=desDec,desMapFrag keepend
syn region desClear start=/^CLEAR:\s*/ end=/$/ contains=desDec,desSubstArg keepend
syn region desColourline start=/^\(COLOUR\|L[A-Z]*COL\):/ end=/$/ contains=desDec,desColour keepend

" Absolutely always highlight the vault name as just that
syn region desNameline start=/^NAME:/ end=/$/ contains=desDec,desVaultname keepend
syn match desVaultname /\w*/ contained

syn match desGod contained /ashenzari\|beogh\|cheibriados\|dithmenos\|elyvilon/
syn match desGod contained /fedhas\|gozag\|jiyva\|kikubaaqudgha\|lugonu/
syn match desGod contained /makhleb\|nemelex_xobeh\|okawaru\|qazlal\|ru/
syn match desGod contained /sif_muna\|trog\|vehumet\|xom\|yredelemnul\|zin/
syn match desGod contained /the_shining_one/

syn keyword desDeclarator ORIENT: DEPTH: PLACE: MONS: FLAGS: default-depth: TAGS: CHANCE: WEIGHT:
syn keyword desDeclarator ITEM: KFEAT: KMONS: KITEM: KMASK: KPROP: MARKER: WELCOME: LFLAGS: BFLAGS:
syn keyword desDeclarator LFLOORTILE: LROCKTILE: FTILE: RTILE: TILE: SUBVAULT: FHEIGHT: DESC: ORDER:

" keywords
" ORIENT
syn keyword desOrientation north south east west northwest northeast southwest southeast encompass float centre

" DEPTH | PLACE
syn keyword desOrientation Depths Temple Orc Elf Lair Swamp Shoals Snake Spider Slime Vaults Blade Crypt Tomb
"Note: `Zot` totally highlights in e.g. the items `rune of Zot` and `Orb of Zot`. Not worth fixing.
syn keyword desOrientation Hell Dis Geh Coc Tar Zot Forest Abyss Pan Zig Lab Bazaar Trove Sewer Ossuary
syn keyword desOrientation Bailey IceCv Volcano WizLab

"Note: This is the list above, but lower-cased
syn match desBranch contained /d\|temple\|orc\|elf\|lair\|swamp\|shoals\|snake\|spider\|slime\|vaults\|blade\|crypt\|tomb/
syn match desBranch contained /hell\|dis\|geh\|coc\|tar\|zot\|forest\|abyss\|pan\|zig\|lab\|bazaar\|trove\|sewer\|ossuary/
syn match desBranch contained /bailey\|icecv\|volcano\|wizlab/

syn match desBranchname contained /dungeon\|depths\|temple\|orcish_mines\|elven_halls\|lair\|swamp\|shoals/
syn match desBranchname contained /snake_pit\|spider_nest\|slime_pits\|vaults\|hall_of_blades\|crypt\|tomb/
syn match desBranchname contained /hell\|dis\|gehenna\|cocytus\|tartarus\|zot\|forest\|abyss\|pandemonium/
syn match desBranchname contained /ziggurat\|labyrinth\|bazaar\|trove\|sewer\|ossuary/
syn match desBranchname contained /bailey\|ice_cave\|volcano\|wizlab/

" TAGS
" in abyss.cc
syn keyword desOrientation abyss_exit
" in decks.cc and dgn-labyrinth.cc (without `minotaur` because monster)
syn keyword desOrientation lab generate_loot
" from dlua.ziggurat
syn keyword desOrientation ziggurat_pillar centered
" map building in dungeon.cc (`transparent` is handled later)
"Note: `dummy` mis-catches `training dummy` about half as often as actually used as tag
syn keyword desOrientation dummy arrival no_exits extra ruin layout pan decor
syn keyword desOrientation allow_dup uniq luniq
syn keyword desOrientation no_hmirror no_vmirror no_rotate
syn keyword desOrientation no_dump
" vault placement in maps.cc
syn keyword desOrientation unrand place_unique special_room tutorial
syn keyword desOrientation water_ok overwrite_floor_cell replace_portal
" V vault building (mostly dlua/v_layouts and v_rooms)
syn keyword desOrientation vaults_room vaults_empty vaults_hard no_windows preserve_wall

" LFLAGS (in l_dgn.cc)
syn keyword desOrientation no_tele_control not_mappable no_magic_map

" ITEM | KITEM (in mapdef.cc, without `random`)
syn keyword desOrientation randbook any good_item star_item superb_item gold nothing
syn keyword desOrientation acquire mundane damaged cursed randart not_cursed useful unobtainable
syn keyword desOrientation mimic no_mimic no_pickup no_uniq allow_uniq
"Note: `rotting` removed here which often caught `rotting devil` but was unused as item tag
syn keyword desOrientation corpse chunk skeleton never_decay

" MONS | KMONS (in mapdef.cc)
syn keyword desOrientation fix_slot priest_spells actual_spells god_gift
syn keyword desOrientation generate_awake patrolling band
syn keyword desOrientation hostile friendly good_neutral fellow_slime strict_neutral neutral
"Note: `spectre` removed: mis-catches `silent spectre` but was unused as modifier (`spectral` exists)
syn keyword desOrientation zombie skeleton simulacrum spectral chimera
syn keyword desOrientation seen always_corpse never_corpse
syn keyword desOrientation base nonbase
syn keyword desOrientation n_suf        n_adj           n_rpl         n_the
syn keyword desOrientation name_suffix  name_adjective  name_replace  name_definite
syn keyword desOrientation n_des            n_spe         n_zom        n_noc
syn keyword desOrientation name_descriptor  name_species  name_zombie  name_nocorpse

" COLOUR
" Base
syn keyword desColour contained blue      green      cyan      red      magenta      brown  darkgrey
syn keyword desColour contained lightblue lightgreen lightcyan lightred lightmagenta yellow lightgrey white
" Elemental
syn keyword desColour contained fire ice earth electricity air poison water magic mutagenic warp enchant
syn keyword desColour contained heal holy dark death unholy vehumet beogh crystal blood smoke slime jewel
syn keyword desColour contained elven dwarven orcish flash kraken floor rock mist shimmer_blue decay
syn keyword desColour contained silver gold iron bone elven_brick waves tree mangrove tornado liquefied
syn keyword desColour contained orb_glow disjunction random

" TILE
syn keyword desOrientation no_random

" KFEAT
syn keyword desOrientation known mimic

" abyss TAGS in mapdef.cc
syn keyword desOrientation abyss abyss_rune
syn keyword desOrientation overwritable
" KMASK (in mapdef.cc)
syn keyword desOrientation vault no_item_gen no_monster_gen no_pool_fixup no_wall_fixup opaque no_trap_gen

" KPROP
syn keyword desOrientation bloody highlight mold no_cloud_gen no_rtele_into no_ctele_into no_tele_into no_submerge no_tide no_jiyva

syn match desComment "^\s*#.*$"

"Note: `;` and `|` are necessary due to monster/randbook `spells:`,
" `.` can be an empty spell slot and `'` is contained in certain spell names,
" `$` and `-` are used in depth definitions (but `,` should not match there).
syn match desProperty /\w*:[[:alnum:]_\.';|\$-]\+/ contains=desAttribute
" Without `oneline` this wraps around and matches e.g. some SUBST: on the next line
syn region desAttribute start=/\</ end=/:/ contained oneline

syn match desEntry "\<\w*_entry\>" contains=desBranch
syn match desEntry "\<serial_\w*\>"
syn match desEntry "\<no_species_\w\w\>"
syn match desEntry "\<\(no\)\=layout_\w*\>"
syn match desEntry "\<l\=uniq_\w*\>"
syn match desEntry "\<chance_\w*\>"
syn match desEntry "\<fallback_\w*\>"
syn match desEntry "\<vaults_entry_\w*\>" contains=desBranch
syn match desEntry "\<vaults_orient_\w\>"
syn match desEntry "\<altar_\w*\>"           contains=desGod
syn match desEntry "\<uniq_altar_\w*\>"      contains=desGod
syn match desEntry "\<temple_overflow_\w*\>" contains=desGod
syn match desEntry "\<overflow_altar_\w*\>"  contains=desGod
syn match desEntry "\<enter_\w*\>"       contains=desBranchname
syn match desEntry "\<exit_\w*\>"        contains=desBranchname
syn match desEntry "\<ruin_\w*\>"        contains=desBranchname

" 'transparent' is a Vim syntax keyword
syn match desTransparent "\<transparent\>"
syn match desRange "\d*-\d*"
syn match desNumber "\s\d*"
syn match desWeight "w\(eight\)\=:\d*"
syn match desWeight "q:\d*\(-\d*\)\="
syn match desSlash "/"

syn keyword desMapBookend MAP ENDMAP contained
syn match desMapWall /x/ contained
syn match desMapPermaWall /X/ contained
syn match desMapStoneWall /c/ contained
syn match desMapGlassWall /[mno]/ contained
syn match desMapMetalWall /v/ contained
syn match desMapCrystalWall /b/ contained
syn match desMapTree /t/ contained

syn match desMapFloor /\./ contained
syn match desMapDoor /[+=]/ contained

syn match desMapShallow /W/ contained
syn match desMapWater /w/ contained
syn match desMapLava /l/ contained

syn match desMapEntry /@/ contained
syn match desMapStairs /[}{)(\]\[]/ contained
syn match desMapTrap /[\^~]/ contained

syn match desMapGold /\$/ contained
syn match desMapValuable /[%*|]/ contained

syn match desMapMonst /[0-9]/ contained

syn cluster desMapElements contains=desMapBookend
syn cluster desMapElements add=desMapWall,desMapPermaWall,desMapStoneWall,desMapGlassWall,desMapCrystalWall,desMapMetalWall,desMapTree
syn cluster desMapElements add=desMapFloor,desMapDoor
syn cluster desMapElements add=desMapShallow,desMapWater,desMapLava
syn cluster desMapElements add=desMapEntry,desMapStairs,desMapTrap
syn cluster desMapElements add=desMapGold,desMapValuable
syn cluster desMapElements add=desMapMons

syn match desSubstArg /\S/ contained nextgroup=desSubstSep skipwhite
syn match desSubstSep /[:=]/ contained nextgroup=desMapFrag skipwhite
syn match desColourSep /[:=]/ contained nextgroup=desColour skipwhite
syn region desMapFrag start=/./ end=/$/ contains=@desMapElements contained

syn region desMap start=/^\s*\<MAP\>\s*$/ end=/^\s*\<ENDMAP\>\s*$/ contains=@desMapElements keepend

hi link desDec        Statement
hi link desDeclarator Statement
hi link desVaultname  Identifier
hi link desMapBookend Statement
hi link desLuaBlock   Statement
hi link desOtherLuaBlock Statement
hi link desLuaBlockEnd Statement
"hi link desColonLine  Statement
hi link desComment    Comment
hi link desMap        String
hi link desSubstArg   String
hi link desRange      String
hi link desEntry      Type
hi link desNumber     String
hi link desWeight     String
hi link desSlash      Comment

hi link desSubstSep    Type
hi link desOrientation Type
hi link desAttribute   Type
hi link desProperty    Special
hi link desGod         Special
hi link desBranch      Special
hi link desBranchname  Special
hi link desColour      Type
hi link desTransparent Type

" It would be really nice if this worked for people who switch bg
" post-loading, like "normal" highlights do. Does someone know how?
if &bg == "dark"
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=white
  hi desMapPermaWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapStoneWall guifg=black gui=bold ctermfg=gray
  hi desMapGlassWall guifg=lightcyan ctermfg=lightcyan
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=lightblue
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapTree guifg=#00aa00 ctermfg=darkgreen
  hi desMapFloor guifg=#008000 ctermfg=darkgray
  hi desMapDoor guifg=brown gui=bold ctermfg=white
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapLava guifg=red gui=bold ctermfg=darkred

  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black
  hi desMapStairs guifg=orange gui=bold ctermfg=magenta
  hi desMapTrap guifg=red gui=bold ctermfg=darkred

  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapValuable guifg=darkgreen gui=bold ctermfg=yellow
  hi desMapMonst guifg=red ctermfg=red
else
  hi desMapWall guifg=darkgray term=bold gui=bold ctermfg=brown
  hi desMapPermaWall guifg=#a0a000 gui=bold ctermfg=yellow
  hi desMapStoneWall guifg=black gui=bold ctermfg=darkgray
  hi desMapGlassWall guifg=lightcyan ctermfg=lightcyan
  hi desMapMetalWall guifg=#004090 term=bold gui=bold ctermfg=blue
  hi desMapCrystalWall guifg=#009040 term=bold gui=bold ctermfg=green
  hi desMapTree guifg=#00aa00 ctermfg=darkgreen
  hi desMapFloor guifg=#008000 ctermfg=lightgray
  hi desMapDoor guifg=brown gui=bold ctermfg=black ctermbg=brown
  hi desMapShallow guifg=lightcyan ctermfg=darkcyan
  hi desMapWater guifg=lightblue ctermfg=darkblue
  hi desMapLava guifg=red gui=bold ctermfg=red

  hi desMapEntry guifg=black guibg=white gui=bold ctermfg=white ctermbg=black
  hi desMapStairs guifg=orange gui=bold ctermfg=white
  hi desMapTrap guifg=red gui=bold ctermfg=red

  hi desMapGold guifg=#c09000 ctermfg=yellow
  hi desMapValuable guifg=darkgreen gui=bold ctermfg=lightgreen
  hi desMapMonst guifg=red ctermfg=darkred
endif

syn sync minlines=45

let b:current_syntax="levdes"
