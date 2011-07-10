// TODO: Generate this automatically from enum.h?

// Goto regions
GOTO_CRT = 0;
GOTO_MSG = 1;
GOTO_STAT = 2;

// Cursors
CURSOR_MOUSE = 0;
CURSOR_TUTORIAL = 1;
CURSOR_MAP = 2;
CURSOR_MAX = 3;

// Foreground flags

// 3 mutually exclusive flags for attitude.
TILE_FLAG_ATT_MASK   = 0x00001800;
TILE_FLAG_PET        = 0x00000800;
TILE_FLAG_GD_NEUTRAL = 0x00001000;
TILE_FLAG_NEUTRAL    = 0x00001800;

TILE_FLAG_S_UNDER    = 0x00002000;
TILE_FLAG_FLYING     = 0x00004000;
TILE_FLAG_STAB       = 0x00008000;
TILE_FLAG_MAY_STAB   = 0x00010000;
TILE_FLAG_NET        = 0x00020000;
TILE_FLAG_POISON     = 0x00040000;
TILE_FLAG_ANIM_WEP   = 0x00080000;
TILE_FLAG_MIMIC      = 0x00100000;
TILE_FLAG_FLAME      = 0x00200000;
TILE_FLAG_BERSERK    = 0x00400000;

// MDAM has 5 possibilities; so uses 3 bits.
TILE_FLAG_MDAM_MASK  = 0x03800000;
TILE_FLAG_MDAM_LIGHT = 0x00800000;
TILE_FLAG_MDAM_MOD   = 0x01000000;
TILE_FLAG_MDAM_HEAVY = 0x01800000;
TILE_FLAG_MDAM_SEV   = 0x02000000;
TILE_FLAG_MDAM_ADEAD = 0x02800000;

// Demon difficulty has 5 possibilities; so uses 3 bits.
TILE_FLAG_DEMON      = 0x34000000;
TILE_FLAG_DEMON_5    = 0x04000000;
TILE_FLAG_DEMON_4    = 0x10000000;
TILE_FLAG_DEMON_3    = 0x14000000;
TILE_FLAG_DEMON_2    = 0x20000000;
TILE_FLAG_DEMON_1    = 0x24000000;

// Background flags
TILE_FLAG_RAY        = 0x00000800;
TILE_FLAG_MM_UNSEEN  = 0x00001000;
TILE_FLAG_UNSEEN     = 0x00002000;
TILE_FLAG_CURSOR1    = 0x00004000;
TILE_FLAG_CURSOR2    = 0x00008000;
TILE_FLAG_CURSOR3    = 0x0000C000;
TILE_FLAG_CURSOR     = 0x0000C000;
TILE_FLAG_TUT_CURSOR = 0x00010000;
TILE_FLAG_TRAV_EXCL  = 0x00020000;
TILE_FLAG_EXCL_CTR   = 0x00040000;
TILE_FLAG_RAY_OOR    = 0x00080000;
TILE_FLAG_OOR        = 0x00100000;
TILE_FLAG_WATER      = 0x00200000;
TILE_FLAG_NEW_STAIR  = 0x00400000;
TILE_FLAG_WAS_SECRET = 0x00800000;

// Kraken tentacle overlays.
TILE_FLAG_KRAKEN_NW  = 0x01000000;
TILE_FLAG_KRAKEN_NE  = 0x02000000;
TILE_FLAG_KRAKEN_SE  = 0x04000000;
TILE_FLAG_KRAKEN_SW  = 0x08000000;

// Eldritch tentacle overlays.
TILE_FLAG_ELDRITCH_NW = 0x10000000;
TILE_FLAG_ELDRITCH_NE = 0x20000000;
TILE_FLAG_ELDRITCH_SE = 0x40000000;
TILE_FLAG_ELDRITCH_SW = 0x80000000;

// General
TILE_FLAG_MASK       = 0x000007FF;


// Minimap features
var val = 0;
MF_UNSEEN = val++;
MF_FLOOR = val++;
MF_WALL = val++;
MF_MAP_FLOOR = val++;
MF_MAP_WALL = val++;
MF_DOOR = val++;
MF_ITEM = val++;
MF_MONS_FRIENDLY = val++;
MF_MONS_PEACEFUL = val++;
MF_MONS_NEUTRAL = val++;
MF_MONS_HOSTILE = val++;
MF_MONS_NO_EXP = val++;
MF_STAIR_UP = val++;
MF_STAIR_DOWN = val++;
MF_STAIR_BRANCH = val++;
MF_FEATURE = val++;
MF_WATER = val++;
MF_LAVA = val++;
MF_TRAP = val++;
MF_EXCL_ROOT = val++;
MF_EXCL = val++;
MF_PLAYER = val++;
MF_MAX = val++;

MF_SKIP = val++;

minimap_colours = [
    "black",       // MF_UNSEEN
    "darkgrey",    // MF_FLOOR
    "grey",        // MF_WALL
    "darkgrey",    // MF_MAP_FLOOR
    "blue",        // MF_MAP_WALL
    "brown",       // MF_DOOR
    "green",       // MF_ITEM
    "lightred",    // MF_MONS_FRIENDLY
    "lightred",    // MF_MONS_PEACEFUL
    "red",         // MF_MONS_NEUTRAL
    "red",         // MF_MONS_HOSTILE
    "darkgreen",   // MF_MONS_NO_EXP
    "blue",        // MF_STAIR_UP
    "magenta",     // MF_STAIR_DOWN
    "cyan",        // MF_STAIR_BRANCH
    "cyan",        // MF_FEATURE
    "grey",        // MF_WATER
    "grey",        // MF_LAVA
    "yellow",      // MF_TRAP
    "darkblue",    // MF_EXCL_ROOT
    "darkcyan",    // MF_EXCL
    "white"        // MF_PLAYER
]
