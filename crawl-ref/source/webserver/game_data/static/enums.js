// TODO: Generate this automatically from enum.h?
define(function () {
    var exports = {};
    // UI States (tileweb.h)
    exports.ui = {};
    exports.ui.NORMAL   = 0;
    exports.ui.CRT      = 1;
    exports.ui.VIEW_MAP = 2;

    // Cursors
    exports.CURSOR_MOUSE = 0;
    exports.CURSOR_TUTORIAL = 1;
    exports.CURSOR_MAP = 2;
    exports.CURSOR_MAX = 3;

    // Halo flags
    exports.HALO_NONE = 0;
    exports.HALO_RANGE = 1;
    exports.HALO_MONSTER = 2;
    exports.HALO_UMBRA = 3;

    // Foreground flags

    // 3 mutually exclusive flags for attitude.
    exports.TILE_FLAG_ATT_MASK   = 0x00001800;
    exports.TILE_FLAG_PET        = 0x00000800;
    exports.TILE_FLAG_GD_NEUTRAL = 0x00001000;
    exports.TILE_FLAG_NEUTRAL    = 0x00001800;

    exports.TILE_FLAG_S_UNDER    = 0x00002000;
    exports.TILE_FLAG_FLYING     = 0x00004000;

    // 3 mutually exclusive flags for behaviour.
    exports.TILE_FLAG_BEH_MASK   = 0x00018000;
    exports.TILE_FLAG_STAB       = 0x00008000;
    exports.TILE_FLAG_MAY_STAB   = 0x00010000;
    exports.TILE_FLAG_FLEEING    = 0x00018000;

    exports.TILE_FLAG_NET        = 0x00020000;
    exports.TILE_FLAG_POISON     = 0x00040000;
    exports.TILE_FLAG_ANIM_WEP   = 0x00080000;
    exports.TILE_FLAG_MIMIC      = 0x00100000;
    exports.TILE_FLAG_STICKY_FLAME = 0x00200000;
    exports.TILE_FLAG_BERSERK    = 0x00400000;
    exports.TILE_FLAG_INNER_FLAME  = 0x40000000;
    exports.TILE_FLAG_CONSTRICTED  = 0x80000000;

    // MDAM has 5 possibilities; so uses 3 bits.
    exports.TILE_FLAG_MDAM_MASK  = 0x03800000;
    exports.TILE_FLAG_MDAM_LIGHT = 0x00800000;
    exports.TILE_FLAG_MDAM_MOD   = 0x01000000;
    exports.TILE_FLAG_MDAM_HEAVY = 0x01800000;
    exports.TILE_FLAG_MDAM_SEV   = 0x02000000;
    exports.TILE_FLAG_MDAM_ADEAD = 0x02800000;

    // Demon difficulty has 5 possibilities; so uses 3 bits.
    exports.TILE_FLAG_DEMON      = 0x34000000;
    exports.TILE_FLAG_DEMON_5    = 0x04000000;
    exports.TILE_FLAG_DEMON_4    = 0x10000000;
    exports.TILE_FLAG_DEMON_3    = 0x14000000;
    exports.TILE_FLAG_DEMON_2    = 0x20000000;
    exports.TILE_FLAG_DEMON_1    = 0x24000000;

    // Background flags
    exports.TILE_FLAG_RAY        = 0x00000800;
    exports.TILE_FLAG_MM_UNSEEN  = 0x00001000;
    exports.TILE_FLAG_UNSEEN     = 0x00002000;
    exports.TILE_FLAG_CURSOR1    = 0x00004000;
    exports.TILE_FLAG_CURSOR2    = 0x00008000;
    exports.TILE_FLAG_CURSOR3    = 0x0000C000;
    exports.TILE_FLAG_CURSOR     = 0x0000C000;
    exports.TILE_FLAG_TUT_CURSOR = 0x00010000;
    exports.TILE_FLAG_TRAV_EXCL  = 0x00020000;
    exports.TILE_FLAG_EXCL_CTR   = 0x00040000;
    exports.TILE_FLAG_RAY_OOR    = 0x00080000;
    exports.TILE_FLAG_OOR        = 0x00100000;
    exports.TILE_FLAG_WATER      = 0x00200000;
    exports.TILE_FLAG_NEW_STAIR  = 0x00400000;
    exports.TILE_FLAG_WAS_SECRET = 0x00800000;

    // Kraken tentacle overlays.
    exports.TILE_FLAG_KRAKEN_NW  = 0x01000000;
    exports.TILE_FLAG_KRAKEN_NE  = 0x02000000;
    exports.TILE_FLAG_KRAKEN_SE  = 0x04000000;
    exports.TILE_FLAG_KRAKEN_SW  = 0x08000000;

    // Eldritch tentacle overlays.
    exports.TILE_FLAG_ELDRITCH_NW = 0x10000000;
    exports.TILE_FLAG_ELDRITCH_NE = 0x20000000;
    exports.TILE_FLAG_ELDRITCH_SE = 0x40000000;
    exports.TILE_FLAG_ELDRITCH_SW = 0x80000000;

    // General
    exports.TILE_FLAG_MASK       = 0x000007FF;

    function rgb(r, g, b)
    {
        return "rgb(" + r + "," + g + "," + b + ")";
    }
    exports.term_colours = [
        rgb(  0,   0,   0),
        rgb(  0,  82, 255),
        rgb(100, 185,  70),
        rgb(  0, 180, 180),
        rgb(255,  48,   0),
        rgb(238,  92, 238),
        rgb(165,  91,   0),
        rgb(162, 162, 162),
        rgb( 82,  82,  82),
        rgb( 82, 102, 255),
        rgb( 82, 255,  82),
        rgb( 82, 255, 255),
        rgb(255,  82,  82),
        rgb(255,  82, 255),
        rgb(255, 255,  82),
        rgb(255, 255, 255),
    ];

    // Menu flags -- see menu.h
    var mf = {};
    mf.NOSELECT         = 0x0000;
    mf.SINGLESELECT     = 0x0001;
    mf.MULTISELECT      = 0x0002;
    mf.NO_SELECT_QTY    = 0x0004;
    mf.ANYPRINTABLE     = 0x0008;
    mf.SELECT_BY_PAGE   = 0x0010;
    mf.ALWAYS_SHOW_MORE = 0x0020;
    mf.NOWRAP           = 0x0040;
    mf.ALLOW_FILTER     = 0x0080;
    mf.ALLOW_FORMATTING = 0x0100;
    mf.SHOW_PAGENUMBERS = 0x0200;
    mf.EASY_EXIT        = 0x1000;
    mf.START_AT_END     = 0x2000;
    mf.PRESELECTED      = 0x4000;
    exports.menu_flag = mf;

    var val = 0;
    exports.CHATTR = {};
    exports.CHATTR.NORMAL = val++;
    exports.CHATTR.STANDOUT = val++;
    exports.CHATTR.BOLD = val++;
    exports.CHATTR.BLINK = val++;
    exports.CHATTR.UNDERLINE = val++;
    exports.CHATTR.REVERSE = val++;
    exports.CHATTR.DIM = val++;
    exports.CHATTR.HILITE = val++;
    exports.CHATTR.ATTRMASK = 0xF;

    // Minimap features
    val = 0;
    exports.MF_UNSEEN = val++;
    exports.MF_FLOOR = val++;
    exports.MF_WALL = val++;
    exports.MF_MAP_FLOOR = val++;
    exports.MF_MAP_WALL = val++;
    exports.MF_DOOR = val++;
    exports.MF_ITEM = val++;
    exports.MF_MONS_FRIENDLY = val++;
    exports.MF_MONS_PEACEFUL = val++;
    exports.MF_MONS_NEUTRAL = val++;
    exports.MF_MONS_HOSTILE = val++;
    exports.MF_MONS_NO_EXP = val++;
    exports.MF_STAIR_UP = val++;
    exports.MF_STAIR_DOWN = val++;
    exports.MF_STAIR_BRANCH = val++;
    exports.MF_FEATURE = val++;
    exports.MF_WATER = val++;
    exports.MF_LAVA = val++;
    exports.MF_TRAP = val++;
    exports.MF_EXCL_ROOT = val++;
    exports.MF_EXCL = val++;
    exports.MF_PLAYER = val++;
    exports.MF_MAX = val++;

    exports.MF_SKIP = val++;

    return exports;
});
