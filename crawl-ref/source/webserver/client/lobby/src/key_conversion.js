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
    var CK_TAB_PLACEHOLDER = val++ // placeholder value, not used

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

    var CK_CTRL_SHIFT_UP = val++
    var CK_CTRL_SHIFT_DOWN = val++
    var CK_CTRL_SHIFT_LEFT = val++
    var CK_CTRL_SHIFT_RIGHT = val++
    var CK_CTRL_SHIFT_INSERT = val++
    var CK_CTRL_SHIFT_HOME = val++
    var CK_CTRL_SHIFT_END = val++
    var CK_CTRL_SHIFT_CLEAR = val++
    var CK_CTRL_SHIFT_PGUP = val++
    var CK_CTRL_SHIFT_PGDN = val++
    var CK_CTRL_SHIFT_TAB = val++

    // placeholders, not used except for math
    var CK_ENTER_PLACEHOLDER = val++
    var CK_BKSP_PLACEHOLDER = val++
    var CK_ESCAPE_PLACEHOLDER = val++
    var CK_DELETE_PLACEHOLDER = val++
    var CK_SPACE_PLACEHOLDER = val++
     // various things here may be hard to enter in some browsers..
    var CK_SHIFT_ENTER = val++
    var CK_SHIFT_BKSP = val++
    var CK_SHIFT_ESCAPE = val++
    var CK_SHIFT_DELETE = val++
    var CK_SHIFT_SPACE = val++
    var CK_CTRL_ENTER = val++
    var CK_CTRL_BKSP = val++
    var CK_CTRL_ESCAPE = val++
    var CK_CTRL_DELETE = val++
    var CK_CTRL_SPACE = val++
    var CK_CTRL_SHIFT_ENTER = val++
    var CK_CTRL_SHIFT_BKSP = val++
    var CK_CTRL_SHIFT_ESCAPE = val++
    var CK_CTRL_SHIFT_DELETE = val++
    var CK_CTRL_SHIFT_SPACE = val++

    // Mouse codes. XX these don't match cio.h?
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
    var ctrlshift_key_conversion = {}
    var legacy_key_conversion = {}
    var code_conversion = {}

    var do_code_conversion = false;

    function enable_code_conversion()
    {
        do_code_conversion = true;
    }

    function code_conversion_enabled()
    {
        return do_code_conversion;
    }

    function reset_keycodes()
    {
        // reset so that older versions that are unaware of this var still
        // work.
        do_code_conversion = false;

        // TODO: keycodes are deprecated in js...
        key_conversion = {
            // XX 13 (enter) is unhandled?? Always handled in other code?
            27: 27, // Escape
            8: 8,   // Backspace
            9: 9,   // Tab
            46: CK_DELETE, // back compat

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
        };

        legacy_key_conversion = {
            // The keycode mappings for function keys here were wrong for a very
            // long time. They should start with -265 to match ncurses keycodes,
            // but the code used numpad internal dcss keycodes for years and
            // no one noticed. In fact, various incorrect changes happened to
            // the crawl binary to accommodate. For this reason, we still need
            // to use these keycodes for old versions. Overridden in 0.27+.
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

            // without special handling, numpad keys just get mapped to their
            // ordinary versions in modern browsers. Leave it that way for
            // legacy purposes.
        }

        // Legacy fixup: this is used pre-0.27 to handle cases where a keyboard
        // sends specialized numpad number keycodes but no keypress events. It's
        // not clear to me when this happens (a previous comment says that it's
        // chrome-specific, but it is definitely not all chrome browsers and not
        // all keyboards). But, it may still happen. The fixup converts the keycodes
        // to number keys. Overridden for 0.27+. In many (most?) browser
        // scenario this happens automatically for pre-0.27.
        if (!$.browser.mozilla)
        {
            for (var i = 0; i <= 9; i++)
            {
                legacy_key_conversion[96 + i] = 48 + i;
            }
        }

        code_conversion = {
            "Delete": CK_DELETE,

            // see cio.h for these codes
            "Numpad0": -1000,
            "Numpad1": -1001,
            "Numpad2": -1002,
            "Numpad3": -1003,
            "Numpad4": -1004,
            "Numpad5": -1005,
            "Numpad6": -1006,
            "Numpad7": -1007,
            "Numpad8": -1008,
            "Numpad9": -1009,
            "NumpadEnter": -1010,
            "NumpadDivide": -1012,
            "NumpadMultiply": -1015,
            "NumpadAdd": -1016,
            "NumpadSubtract": -1018,
            "NumpadDecimal": -1019,
            "NumpadEqual": -1021,
            // XX NumpadComma?
            "F1": -265,
            "F2": -266,
            "F3": -267,
            "F4": -268,
            "F5": -269,
            "F6": -270,
            "F7": -271,
            "F8": -272,
            "F9": -273,
            "F10": -274,
            // reserve F11 for the browser (full screen)
            // reserve F12 for toggling chat
            "F13": -277,
            // F13 may also show up as printscr, but we don't want to bind that
            "F14": -278,
            "F15": -279,
            "F16": -280,
            "F17": -281,
            "F18": -282,
            "F19": -283,
        }

        if ($.browser.opera)
        {
            // TODO is this still a thing?
            // Opera uses 107 for keydown for =/+, but reports 43 ('+') in the
            // keypress regardless of whether shift is pressed. Unfortunately
            // it also uses 107/43 for numpad +, so we can't distinguish those.
            // We are handling this Opera-specifically so that other browsers
            // can continue to distinguish the two.
            key_conversion[107] = 61;
        }

        // TODO: convert these to use `code` instead of keycodes somehow?
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

            // Numpad on mac / with numlock, matches local tiles behavior in
            // just converting to shifted navigation keys
            97: CK_SHIFT_END,
            98: CK_SHIFT_DOWN,
            99: CK_SHIFT_PGDN,
            100: CK_SHIFT_LEFT,
            102: CK_SHIFT_RIGHT,
            103: CK_SHIFT_HOME,
            104: CK_SHIFT_UP,
            105: CK_SHIFT_PGUP,

            13: CK_SHIFT_ENTER,
            8:  CK_SHIFT_BKSP,
            27: CK_SHIFT_ESCAPE,
            46: CK_SHIFT_DELETE,
            32: CK_SHIFT_SPACE,
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

            // Numpad on mac / with numlock
            97: CK_CTRL_END,
            98: CK_CTRL_DOWN,
            99: CK_CTRL_PGDN,
            100: CK_CTRL_LEFT,
            102: CK_CTRL_RIGHT,
            103: CK_CTRL_HOME,
            104: CK_CTRL_UP,
            105: CK_CTRL_PGUP,

            13: CK_CTRL_ENTER,
            8:  CK_CTRL_BKSP,
            27: CK_CTRL_ESCAPE,
            46: CK_CTRL_DELETE,
            32: CK_CTRL_SPACE,
        };

        ctrlshift_key_conversion = {
            // Numpad / Arrow keys
            45: CK_CTRL_SHIFT_INSERT,
            35: CK_CTRL_SHIFT_END,
            40: CK_CTRL_SHIFT_DOWN,
            34: CK_CTRL_SHIFT_PGDN,
            37: CK_CTRL_SHIFT_LEFT,
            12: CK_CTRL_SHIFT_CLEAR,
            39: CK_CTRL_SHIFT_RIGHT,
            36: CK_CTRL_SHIFT_HOME,
            38: CK_CTRL_SHIFT_UP,
            33: CK_CTRL_SHIFT_PGUP,

            // Numpad on mac / with numlock
            97: CK_CTRL_SHIFT_END,
            98: CK_CTRL_SHIFT_DOWN,
            99: CK_CTRL_SHIFT_PGDN,
            100: CK_CTRL_SHIFT_LEFT,
            102: CK_CTRL_SHIFT_RIGHT,
            103: CK_CTRL_SHIFT_HOME,
            104: CK_CTRL_SHIFT_UP,
            105: CK_CTRL_SHIFT_PGUP,

            13: CK_CTRL_SHIFT_ENTER,
            8:  CK_CTRL_SHIFT_BKSP,
            27: CK_CTRL_SHIFT_ESCAPE,
            46: CK_CTRL_SHIFT_DELETE,
            32: CK_CTRL_SHIFT_SPACE,
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
        ctrlshift: ctrlshift_key_conversion,
        codes: code_conversion,
        legacy: legacy_key_conversion,
        captured_control_keys: captured_control_keys,
        reset_keycodes: reset_keycodes,
        enable_code_conversion: enable_code_conversion,
        code_conversion_enabled: code_conversion_enabled
    }
});
