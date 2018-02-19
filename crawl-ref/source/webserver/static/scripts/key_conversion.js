define(function() {
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

    var key_conversion = {
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
    };

    if (!$.browser.mozilla)
    {
        // Numpad with numlock -- FF sends keypresses, Chrome doesn't
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

    var shift_key_conversion = {
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
    }

    var ctrl_key_conversion = {
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
    }

    var captured_control_keys = [
        "O", "Q", "F", "P", "W", "A", "T", "X", "S", "G", "I", "D", "E",
        "H", "J", "K", "L", "Y", "U", "B", "N", "C"
    ];

    return {
        simple: key_conversion,
        shift: shift_key_conversion,
        ctrl: ctrl_key_conversion,
        captured_control_keys: captured_control_keys
    }
});
