// TODO: Generate this automatically from enum.h?
define(function () {
    var exports = {}, val;
    // Various constants
    exports.gxm = 80;
    exports.gym = 70;
    exports.stat_width = 42;

    // UI States (tileweb.h)
    exports.ui = {};
    exports.ui.NORMAL   = 0;
    exports.ui.CRT      = 1;
    exports.ui.VIEW_MAP = 2;

    // Mouse modes
    val = 0;
    exports.mouse_mode = {};
    exports.mouse_mode.NORMAL = val++;
    exports.mouse_mode.COMMAND = val++;
    exports.mouse_mode.TARGET = val++;
    exports.mouse_mode.TARGET_DIR = val++;
    exports.mouse_mode.TARGET_PATH = val++;
    exports.mouse_mode.MORE = val++;
    exports.mouse_mode.MACRO = val++;
    exports.mouse_mode.PROMPT = val++;
    exports.mouse_mode.YESNO = val++;
    exports.mouse_mode.MAX = val++;

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

    // equipment_type
    exports.equip = {};
    val = 0;
    exports.equip.WEAPON = val++;
    exports.equip.CLOAK = val++;
    exports.equip.HELMET = val++;
    exports.equip.GLOVES = val++;
    exports.equip.BOOTS = val++;
    exports.equip.SHIELD = val++;
    exports.equip.BODY_ARMOUR = val++;
    exports.equip.LEFT_RING = val++;
    exports.equip.RIGHT_RING = val++;
    exports.equip.AMULET = val++;
    exports.equip.RING_ONE = val++;
    exports.equip.RING_TWO = val++;
    exports.equip.RING_THREE = val++;
    exports.equip.RING_FOUR = val++;
    exports.equip.RING_FIVE = val++;
    exports.equip.RING_SIX = val++;
    exports.equip.RING_SEVEN = val++;
    exports.equip.RING_EIGHT = val++;
    exports.equip.NUM_EQUIP = val++;

    // Tile flags.
    // Mostly this complicated because they need more than 32 bits.

    function array_and(arr1, arr2)
    {
        var result = [];
        for (var i = 0; i < arr1.length && i < arr2.length; ++i)
        {
            result.push(arr1[i] & arr2[i]);
        }
        return result;
    }

    function array_equal(arr1, arr2)
    {
        // return (arr1 <= arr2) && (arr1 >= arr2);
        for (var i = 0; i < arr1.length || i < arr2.length; ++i)
        {
            if ((arr1[i] || 0) != (arr2[i] || 0))
                return false;
        }
        return true;
    }

    function array_nonzero(arr)
    {
        for (var i = 0; i < arr.length; ++i)
        {
            if (arr[i] != 0) return true;
        }
        return false;
    }

    function prepare_flags(tileidx, flagdata)
    {
        if (!isNaN(tileidx))
            tileidx = [tileidx];
        else if (tileidx.value !== undefined)
            return tileidx;
        while (tileidx.length < 2) tileidx.push(0);

        for (var flagname in flagdata.flags)
        {
            var flagmask = flagdata.flags[flagname];
            if (isNaN(flagmask))
                tileidx[flagname] = array_nonzero(array_and(tileidx, flagmask));
            else
                tileidx[flagname] = (tileidx[0] & flagmask) != 0;
        }

        for (var i = 0; i < flagdata.exclusive_flags.length; ++i)
        {
            var excl = flagdata.exclusive_flags[i];
            var val;
            if (isNaN(excl.mask))
                val = array_and(tileidx, excl.mask);
            else
                val = [tileidx[0] & excl.mask];

            for (var flagname in excl)
            {
                if (flagname === "mask") continue;
                if (isNaN(excl[flagname]))
                    tileidx[flagname] = array_equal(val, excl[flagname]);
                else
                    tileidx[flagname] = (val[0] == excl[flagname]);
            }
        }

        tileidx.value = tileidx[0] & flagdata.mask;
        return tileidx;
    }

    /* Hex literals are signed, so values with the highest bit set
       would have to be written in 2-complement; this way is easier to
       read */
    var highbit = 1 << 31;

    // Foreground flags

    // 3 mutually exclusive flags for attitude.
    var fg_flags = { flags: {}, exclusive_flags: [] };
    fg_flags.exclusive_flags.push({
        mask       : 0x00030000,
        PET        : 0x00010000,
        GD_NEUTRAL : 0x00020000,
        NEUTRAL    : 0x00030000,
    });

    fg_flags.flags.S_UNDER = 0x00040000;
    fg_flags.flags.FLYING  = 0x00080000;

    // 3 mutually exclusive flags for behaviour.
    fg_flags.exclusive_flags.push({
        mask       : 0x00300000,
        STAB       : 0x00100000,
        MAY_STAB   : 0x00200000,
        FLEEING    : 0x00300000,
    });

    fg_flags.flags.NET          = 0x00400000;
    fg_flags.flags.POISON       = 0x00800000;
    fg_flags.flags.WEB          = 0x01000000;
    fg_flags.flags.GLOWING      = 0x02000000;
    fg_flags.flags.STICKY_FLAME = 0x04000000;
    fg_flags.flags.BERSERK      = 0x08000000;
    fg_flags.flags.INNER_FLAME  = 0x10000000;
    fg_flags.flags.CONSTRICTED  = 0x20000000;
    fg_flags.flags.SLOWED       = [0, 0x080];
    fg_flags.flags.PAIN_MIRROR  = [0, 0x100];
    fg_flags.flags.HASTED       = [0, 0x200];
    fg_flags.flags.MIGHT        = [0, 0x400];
    fg_flags.flags.PETRIFYING   = [0, 0x800];
    fg_flags.flags.PETRIFIED    = [0, 0x1000];
    fg_flags.flags.BLIND        = [0, 0x2000];
    fg_flags.flags.ANIM_WEP     = [0, 0x4000];
    fg_flags.flags.SUMMONED     = [0, 0x8000];

    // MDAM has 5 possibilities, so uses 3 bits.
    fg_flags.exclusive_flags.push({
        mask       : [0x40000000 | highbit, 0x01],
        MDAM_LIGHT : [0x40000000, 0x00],
        MDAM_MOD   : [highbit, 0x00],
        MDAM_HEAVY : [0x40000000 | highbit, 0x00],
        MDAM_SEV   : [0x00000000, 0x01],
        MDAM_ADEAD : [0x40000000 | highbit, 0x01],
    });

    // Demon difficulty has 5 possibilities, so uses 3 bits.
    fg_flags.exclusive_flags.push({
        mask       : [0, 0x0E],
        DEMON_5    : [0, 0x02],
        DEMON_4    : [0, 0x04],
        DEMON_3    : [0, 0x06],
        DEMON_2    : [0, 0x08],
        DEMON_1    : [0, 0x0E],
    });

    // Mimics, 2 bits.
    fg_flags.exclusive_flags.push({
        mask        : [0, 0x60],
        MIMIC_INEPT : [0, 0x20],
        MIMIC       : [0, 0x40],
        MIMIC_RAVEN : [0, 0x60],
    });

    fg_flags.mask             = 0x0000FFFF;

    // Background flags
    var bg_flags = { flags: {}, exclusive_flags: [] };
    bg_flags.flags.RAY        = 0x00010000;
    bg_flags.flags.MM_UNSEEN  = 0x00020000;
    bg_flags.flags.UNSEEN     = 0x00040000;
    bg_flags.exclusive_flags.push({
        mask       : 0x00180000,
        CURSOR1    : 0x00180000,
        CURSOR2    : 0x00080000,
        CURSOR3    : 0x00100000,
    });
    bg_flags.flags.TUT_CURSOR = 0x00200000;
    bg_flags.flags.TRAV_EXCL  = 0x00400000;
    bg_flags.flags.EXCL_CTR   = 0x00800000;
    bg_flags.flags.RAY_OOR    = 0x01000000;
    bg_flags.flags.OOR        = 0x02000000;
    bg_flags.flags.WATER      = 0x04000000;
    bg_flags.flags.NEW_STAIR  = 0x08000000;

    // Kraken tentacle overlays.
    bg_flags.flags.KRAKEN_NW  = 0x20000000;
    bg_flags.flags.KRAKEN_NE  = 0x40000000;
    bg_flags.flags.KRAKEN_SE  = highbit;
    bg_flags.flags.KRAKEN_SW  = [0, 0x01];

    // Eldritch tentacle overlays.
    bg_flags.flags.ELDRITCH_NW = [0, 0x02];
    bg_flags.flags.ELDRITCH_NE = [0, 0x04];
    bg_flags.flags.ELDRITCH_SE = [0, 0x08];
    bg_flags.flags.ELDRITCH_SW = [0, 0x10];
    bg_flags.flags.LANDING     = [0, 0x200];
    bg_flags.mask              = 0x0000FFFF;

    exports.prepare_fg_flags = function (tileidx)
    {
        return prepare_flags(tileidx, fg_flags);
    }
    exports.prepare_bg_flags = function (tileidx)
    {
        return prepare_flags(tileidx, bg_flags);
    }

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

    val = 0;
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

    exports.reverse_lookup = function (e, value) {
        for (var prop in e)
        {
            if (e[prop] == value)
                return prop;
        }
    };

    return exports;
});
