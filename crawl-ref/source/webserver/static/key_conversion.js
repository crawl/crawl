var val;
// Key codes - from cio.h
val = -255
CK_DELETE = val++

CK_UP = val++
CK_DOWN = val++
CK_LEFT = val++
CK_RIGHT = val++

CK_INSERT = val++

CK_HOME = val++
CK_END = val++
CK_CLEAR = val++

CK_PGUP = val++
CK_PGDN = val++
CK_TAB_TILE = val++ // unused

CK_SHIFT_UP = val++
CK_SHIFT_DOWN = val++
CK_SHIFT_LEFT = val++
CK_SHIFT_RIGHT = val++

CK_SHIFT_INSERT = val++

CK_SHIFT_HOME = val++
CK_SHIFT_END = val++
CK_SHIFT_CLEAR = val++

CK_SHIFT_PGUP = val++
CK_SHIFT_PGDN = val++
CK_SHIFT_TAB = val++

CK_CTRL_UP = val++
CK_CTRL_DOWN = val++
CK_CTRL_LEFT = val++
CK_CTRL_RIGHT = val++

CK_CTRL_INSERT = val++

CK_CTRL_HOME = val++
CK_CTRL_END = val++
CK_CTRL_CLEAR = val++

CK_CTRL_PGUP = val++
CK_CTRL_PGDN = val++
CK_CTRL_TAB = val++

// Mouse codes.
val = -10009
CK_MOUSE_MOVE  = val++
CK_MOUSE_CMD = val++
CK_MOUSE_B1 = val++
CK_MOUSE_B2 = val++
CK_MOUSE_B3 = val++
CK_MOUSE_B4 = val++
CK_MOUSE_B5 = val++
CK_MOUSE_CLICK = val++

var key_conversion = {
    // Escape
    27: 27,
    // Backspace
    8: 8,
    // Tab
    9: 9,

    // Numpad / Arrow keys
    35: -1001,
    40: -253,//-1002,
    34: -1003,
    37: -252,//-1004,
    39: -251,//-1006,
    36: -1007,
    38: -254,//-1008,
    33: -1009,

    // Numpad with numlock
    96: 48,
    97: 49,
    98: 50,
    99: 51,
    100: 52,
    101: 53,
    102: 54,
    103: 55,
    104: 56,
    105: 57,

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
    123: -1022
};

var shift_key_conversion = {
    // Numpad / Arrow keys
    35: CK_SHIFT_END,
    40: CK_SHIFT_DOWN,
    34: CK_SHIFT_PGDN,
    37: CK_SHIFT_LEFT,
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
    35: CK_CTRL_END,
    40: CK_CTRL_DOWN,
    34: CK_CTRL_PGDN,
    37: CK_CTRL_LEFT,
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
