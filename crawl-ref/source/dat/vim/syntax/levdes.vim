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
" * In your .vimrc, or in a new file ~/.vim/ftdetect/levdes.vim, add this line:
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
syn keyword desDec contained CLEAR: COLOUR: NAME: NSUBST: MARKER: SHUFFLE:
syn keyword desDec contained LROCKCOL: LFLOORCOL: SUBST:

syn region desMark start=/^MARKER:/ end=/$/ contains=desDec keepend
syn region desMark start=/^MARKER:\s*\S\+\s*[=:]\s*lua:/ skip=/\\\s*/ end=/$/ contains=desDec,@desLuagroup,@deslua keepend
syn region desSubst start=/^SUBST:\s*/ end=/$/ contains=desDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desNsubst start=/^NSUBST:\s*/ end=/$/ contains=desDec,desSubstArg,desSubstSep,@desMapElements keepend
syn region desShuffle start=/^SHUFFLE:\s*/ end=/$/ contains=desDec,desMapFrag keepend
syn region desClear start=/^CLEAR:\s*/ end=/$/ contains=desDec,desSubstArg keepend
syn region desColourline start=/^\(COLOUR\|L[A-Z]*COL\):/ end=/$/ contains=desDec,desColour keepend

" Absolutely always highlight the vault name as just that
syn region desNameline start=/^NAME:/ end=/$/ contains=desDec,desVaultname keepend
syn match desVaultname /\w*/ contained

syn match desGod contained /ashenzari\|beogh\|cheibriados\|dithmenos\|elyvilon/
syn match desGod contained /fedhas\|gozag\|hepliaklqana\|jiyva\|kikubaaqudgha/
syn match desGod contained /lugonu\|makhleb\|nemelex_xobeh\|okawaru\|pakellas/
syn match desGod contained /qazlal\|ru\|sif_muna\|trog\|uskayaw\|vehumet/
syn match desGod contained /xom\|yredelemnul\|zin\|the_shining_one\|wu_jian/

syn keyword desDeclarator ORIENT: DEPTH: PLACE: MONS: FLAGS: default-depth:
syn keyword desDeclarator TAGS: CHANCE: WEIGHT: ITEM: KFEAT: KMONS: KITEM:
syn keyword desDeclarator KMASK: KPROP: WELCOME: LFLOORTILE: LROCKTILE: FTILE:
syn keyword desDeclarator RTILE: TILE: SUBVAULT: FHEIGHT: DESC: ORDER:

" keywords
" ORIENT
syn keyword desOrientation north south east west northwest northeast southwest
syn keyword desOrientation southwest southeast encompass float centre

" DEPTH | PLACE
"Note: `Zot` totally highlights in e.g. the items `rune of Zot` and `Orb of
"Zot`. Not worth fixing.
syn keyword desOrientation Abyss Bailey Bazaar Coc Crypt D Depths Desolation Dis
syn keyword desOrientation Elf Geh Hell IceCv Lair Gauntlet Orc Ossuary Pan Sewer
syn keyword desOrientation Shoals Slime Snake Spider Swamp Tar Temple Tomb Trove
syn keyword desOrientation Vaults Volcano Wizlab Zot Zig

"Note: This is the list above, but lower-cased
syn match desBranch contained /abyss\|bailey\|bazaar\|coc\|crypt\|d\|depths/
syn match desBranch contained /desolation\|dis\|elf\|geh\|hell\|icecv\|lab/
syn match desBranch contained /lair\|orc\|ossuary\|pan\|sewer\|shoals\|slime/
syn match desBranch contained /snake\|spider\|swamp\|tar\|temple\|tomb\|trove/
syn match desBranch contained /vaults\|volcano\|wizlab\|zig\|zot/

syn match desBranchname contained /abyss\|bailey\|bazaar\|cocytus\|crypt/
syn match desBranchname contained /depths\|desolation\|dis\|dungeon/
syn match desBranchname contained /elven_halls\|forest\|gehenna\|hell/
syn match desBranchname contained /ice_cave\|labyrinth\|lair\|orcish_mines/
syn match desBranchname contained /ossuary\|pandemonium\|sewer\|shoals/
syn match desBranchname contained /slime_pits\|snake_pit\|spider_nest\|swamp/
syn match desBranchname contained /tartarus\|temple\|tomb\|trove\|vaults/
syn match desBranchname contained /volcano\|wizlab\|ziggurat\|zot/

" TAGS
" in abyss.cc
syn keyword desOrientation abyss_exit
" from dlua.ziggurat
syn keyword desOrientation ziggurat_pillar centered
" map building in dungeon.cc (`transparent` is handled later)
"Note: `dummy` mis-catches `training dummy` about half as often as actually
"used as tag
syn keyword desOrientation dummy arrival no_exits extra ruin layout pan decor
syn keyword desOrientation allow_dup uniq luniq no_hmirror no_vmirror no_rotate
syn keyword desOrientation no_dump
" vault placement in maps.cc
syn keyword desOrientation unrand place_unique special_room tutorial water_ok
syn keyword desOrientation water_ok overwrite_floor_cell replace_portal
" V vault building (mostly dlua/v_layouts and v_rooms)
syn keyword desOrientation vaults_room vaults_empty vaults_hard no_windows
syn keyword desOrientation preserve_wall

" ITEM | KITEM (in mapdef.cc, without `random`)
syn keyword desOrientation randbook any good_item star_item superb_item gold
syn keyword desOrientation nothing syn keyword desOrientation acquire mundane
syn keyword desOrientation damaged cursed randart not_cursed useful unobtainable
syn keyword desOrientation mimic no_mimic no_pickup no_uniq allow_uniq
"Note: `rotting` removed here which often caught `rotting devil` but was
"unused as item tag
syn keyword desOrientation corpse chunk skeleton never_decay

" MONS | KMONS (in mapdef.cc)
syn keyword desOrientation fix_slot priest_spells actual_spells god_gift
syn keyword desOrientation generate_awake patrolling band hostile friendly
syn keyword desOrientation good_neutral fellow_slime strict_neutral neutral
"Note: `spectre` removed: mis-catches `silent spectre` but was unused as
"modifier (`spectral` exists)
syn keyword desOrientation base nonbase zombie skeleton simulacrum spectral
syn keyword desOrientation seen always_corpse never_corpse
syn keyword desOrientation n_suf        n_adj           n_rpl
syn keyword desOrientation name_suffix  name_adjective  name_replace
syn keyword desOrientation n_des            n_spe         n_zom
syn keyword desOrientation name_descriptor  name_species  name_zombie
syn keyword desOrientation n_noc          n_the
syn keyword desOrientation name_nocorpse  name_definite

" COLOUR
" Base
syn keyword desColour contained blue      green      cyan      red
syn keyword desColour contained lightblue lightgreen lightcyan lightred
syn keyword desColour contained magenta      brown  darkgrey
syn keyword desColour contained lightmagenta yellow lightgrey white
" Elemental
syn keyword desColour contained fire ice earth electricity air poison water
syn keyword desColour contained magic mutagenic warp enchant heal holy dark
syn keyword desColour contained death unholy vehumet beogh crystal blood smoke
syn keyword desColour contained slime jewel elven dwarven orcish flash kraken
syn keyword desColour contained floor rock mist shimmer_blue decay silver gold
syn keyword desColour contained iron bone elven_brick waves tree mangrove
syn keyword desColour contained tornado liquefied orb_glow disjunction random

" TILE
syn keyword desOrientation no_random

" KFEAT
syn keyword desOrientation known mimic

" abyss TAGS in mapdef.cc
syn keyword desOrientation abyss abyss_rune

" Layout tag
syn keyword desOrientation overwritable

" KMASK (in mapdef.cc)
syn keyword desOrientation vault no_item_gen no_monster_gen no_pool_fixup
syn keyword desOrientation no_wall_fixup opaque no_trap_gen

" KPROP
syn keyword desOrientation bloody highlight mold no_cloud_gen no_tele_into
syn keyword desOrientation no_submerge no_tide no_jiyva

syn match desComment "^\s*#.*$&"

"Note: `;` and `|` are necessary due to monster/randbook `spells:`,
" `.` can be an empty spell slot and `'` is contained in certain spell names,
" `$` and `-` are used in depth definitions (but `,` should not match there).
syn match desProperty /\w*:[[:alnum:]_\.';|\$-]\+/ contains=desAttribute
" Without `oneline` this wraps around and matches e.g. some SUBST: on the next
" line
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
syn cluster desMapElements add=desMapWall,desMapPermaWall,desMapStoneWall
syn cluster desMapElements add=desMapGlassWall,desMapCrystalWall
syn cluster desMapElements add=desMapMetalWall,desMapTree,desMapFloor,desMapDoor
syn cluster desMapElements add=desMapShallow,desMapWater,desMapLava
syn cluster desMapElements add=desMapEntry,desMapStairs,desMapTrap
syn cluster desMapElements add=desMapGold,desMapValuable,desMapMons

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

" Highlight of MAP features.
hi link desMapWall        Statement
hi link desMapStoneWall   Type
hi link desMapMetalWall   Type
hi link desMapPermaWall   Define
hi link desMapGlassWall   Constant
hi link desMapCrystalWall Function
hi link desMapTree        String
hi link desMapFloor       Normal
hi link desMapDoor        Special
hi link desMapShallow     Identifier
hi link desMapWater       Define
hi link desMapLava        Error
hi link desMapEntry       Special
hi link desMapStairs      Special
hi link desMapTrap        Special
hi link desMapGold        Operator
hi link desMapValuable    Operator
hi link desMapMonst       String

syn sync minlines=45

let b:current_syntax="levdes"
