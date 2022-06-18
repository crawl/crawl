define(function() {
    // This file is not versioned, which leads to some caveats: it may require
    // a webserver restart to edit, and it needs to work for *all* versions of
    // crawl on the webtiles server. For that reason it doesn't change often
    // or easily. If needed, values here can be overridden in `game.js`, and
    // this is the recommended way to make version-specific tweaks.
    var val;
    // Key codes - from cio.h
    val = -255
    var CK_DELETE = val++

    var CK_UP = val++
    var CK_DOWN = val++
    var CK_LEFT = val++
    var CK_RIGHT = val++

    var CK_INSERT = val++

    var CK_HOME = val++
    var CK_END = val++
    var CK_CLEAR = val++

    var CK_PGUP = val++
    var CK_PGDN = val++
    var CK_TAB_TILE = val++ // unused

    var CK_SHIFT_UP = val++
    var CK_SHIFT_DOWN = val++
    var CK_SHIFT_LEFT = val++
    var CK_SHIFT_RIGHT = val++

    var CK_SHIFT_INSERT = val++

    var CK_SHIFT_HOME = val++
    var CK_SHIFT_END = val++
    var CK_SHIFT_CLEAR = val++

    var CK_SHIFT_PGUP = val++
    var CK_SHIFT_PGDN = val++
    var CK_SHIFT_TAB = val++

    var CK_CTRL_UP = val++
    var CK_CTRL_DOWN = val++
    var CK_CTRL_LEFT = val++
    var CK_CTRL_RIGHT = val++

    var CK_CTRL_INSERT = val++

    var CK_CTRL_HOME = val++
    var CK_CTRL_END = val++
    var CK_CTRL_CLEAR = val++

    var CK_CTRL_PGUP = val++
    var CK_CTRL_PGDN = val++
    var CK_CTRL_TAB = val++

    // Mouse codes.
    val = -10009
    var CK_MOUSE_MOVE  = val++
    var CK_MOUSE_CMD = val++
    var CK_MOUSE_B1 = val++
    var CK_MOUSE_B2 = val++
    var CK_MOUSE_B3 = val++
    var CK_MOUSE_B4 = val++
    var CK_MOUSE_B5 = val++
    var CK_MOUSE_CLICK = val++

    var key_conversion = {}
    var shift_key_conversion = {}
    var ctrl_key_conversion = {}

    function reset_keycodes()
    {
        key_conversion = {
            // Escape
            27: 27,
            // Backspace
            8: 8,
            // Tab
            9: 9,

            // Numpad / Arrow keys
            45: CK_INSERT,
            35: CK_END,
            40: CK_DOWN,
            34: CK_PGDN,
            37: CK_LEFT,
            12: CK_CLEAR,
            39: CK_RIGHT,
            36: CK_HOME,
            38: CK_UP,
            33: CK_PGUP,

            // Function keys
            112: -1011, // F1
            113: -1012,
            114: -1013,
            115: -1014,
            116: -1015,
            117: -1016,
            118: -1017,
            119: -1018,
            120: -1019,
            121: -1020,
            //    122: -1021, // Don't occupy F11, it's used for fullscreen
            //    123: -1022, // used for chat

            // TODO: the above keycodes for function keys are wrong, but have been
            // wrong for a very long time. They produce collisions with a bunch of
            // keycodes that should more naturally be used for numpad keys. Can
            // this be changed here without breaking things? Possibly not.
            // In 0.27+, they are replaced in game.js by:
            // 112: -265, // F1
            // 113: -266,
            // 114: -267,
            // 115: -268,
            // 116: -269,
            // 117: -270,
            // 118: -271,
            // 119: -272,
            // 120: -273,
            // 121: -274,
        };

        // Legacy fixup: this is used pre-0.27 to handle cases where a keyboard
        // sends specialized numpad number keycodes but no keypress events. It's
        // not clear to me when this happens (a previous comment says that it's
        // chrome-specific, but it is definitely not all chrome browsers and not
        // all keyboards). But, it may still happen. The fixup converts the keycodes
        // to number keys. Overridden for 0.27+ in game.js
        if (!$.browser.mozilla)
        {
            for (var i = 0; i <= 9; i++)
            {
                key_conversion[96 + i] = 48 + i;
            }
        }

        if ($.browser.opera)
        {
            // Opera uses 107 for keydown for =/+, but reports 43 ('+') in the
            // keypress regardless of whether shift is pressed. Unfortunately
            // it also uses 107/43 for numpad +, so we can't distinguish those.
            // We are handling this Opera-specifically so that other browsers
            // can continue to distinguish the two.
            key_conversion[107] = 61;
        }

        shift_key_conversion = {
            9: CK_SHIFT_TAB,

            // Numpad / Arrow keys
            45: CK_SHIFT_INSERT,
            35: CK_SHIFT_END,
            40: CK_SHIFT_DOWN,
            34: CK_SHIFT_PGDN,
            37: CK_SHIFT_LEFT,
            12: CK_SHIFT_CLEAR,
            39: CK_SHIFT_RIGHT,
            36: CK_SHIFT_HOME,
            38: CK_SHIFT_UP,
            33: CK_SHIFT_PGUP,

            // Numpad with numlock
            97: CK_SHIFT_END,
            98: CK_SHIFT_DOWN,
            99: CK_SHIFT_PGDN,
            100: CK_SHIFT_LEFT,
            102: CK_SHIFT_RIGHT,
            103: CK_SHIFT_HOME,
            104: CK_SHIFT_UP,
            105: CK_SHIFT_PGUP,
        };

        ctrl_key_conversion = {
            // Numpad / Arrow keys
            45: CK_CTRL_INSERT,
            35: CK_CTRL_END,
            40: CK_CTRL_DOWN,
            34: CK_CTRL_PGDN,
            37: CK_CTRL_LEFT,
            12: CK_CTRL_CLEAR,
            39: CK_CTRL_RIGHT,
            36: CK_CTRL_HOME,
            38: CK_CTRL_UP,
            33: CK_CTRL_PGUP,

            // Numpad with numlock
            97: CK_CTRL_END,
            98: CK_CTRL_DOWN,
            99: CK_CTRL_PGDN,
            100: CK_CTRL_LEFT,
            102: CK_CTRL_RIGHT,
            103: CK_CTRL_HOME,
            104: CK_CTRL_UP,
            105: CK_CTRL_PGUP,
        };
    }

    reset_keycodes();

    var captured_control_keys = [
        "O", "Q", "F", "P", "W", "A", "T", "X", "S", "G", "I", "D", "E",
        "H", "J", "K", "L", "Y", "U", "B", "N", "C", "M",
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0"
    ];

    return {
        simple: key_conversion,
        shift: shift_key_conversion,
        ctrl: ctrl_key_conversion,
        captured_control_keys: captured_control_keys,
        reset_keycodes: reset_keycodes
    }
});
