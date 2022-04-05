define([],
function () {
    "use strict";

    var cols = {
        "black": 0,
        "blue": 1,
        "green": 2,
        "cyan": 3,
        "red": 4,
        "magenta": 5,
        "brown": 6,
        "lightgrey": 7,
        "lightgray": 7,
        "darkgrey": 8,
        "darkgray": 8,
        "lightblue": 9,
        "lightgreen": 10,
        "lightcyan": 11,
        "lightred": 12,
        "lightmagenta": 13,
        "yellow": 14,
        "h": 14,
        "white": 15,
        "w": 15
    };

    function escape_html(str) {
        return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    }

    function formatted_string_to_html(str)
    {
        var other_open = false;
        var filtered = str.replace(/<?<(\/?[a-z]*)>?|>|&/ig, function (str, p1) {
            if (p1 === undefined)
                p1 = "";
            var closing = false;
            if (p1.match(/^\//))
            {
                p1 = p1.substr(1);
                closing = true;
            }
            if (p1 in cols && !str.match(/^<</) && str.match(/>$/))
            {
                if (closing)
                {
                    other_open = false;
                    return "</span>";
                }
                else
                {
                    var text = "<span class='fg" + cols[p1] + "'>";
                    if (other_open)
                        text = "</span>" + text;
                    other_open = true;
                    return text;
                }
            }
            else
            {
                if (str.match(/^<</))
                    return escape_html(str.substr(1));
                else
                {
                    return escape_html(str);
                }
            }
        });
        if (other_open)
            filtered += "</span>";
        return filtered;
    }

    function init_canvas(element, w, h) {
        var ratio = window.devicePixelRatio;
        element.width = w;
        element.height = h;
        element.style.width = (w / ratio) + 'px';
        element.style.height = (h / ratio) + 'px';
    }

    function make_key(x, y) {
        // Zig-zag encode X and Y.
        x = (x << 1) ^ (x >> 31);
        y = (y << 1) ^ (y >> 31);

        // Interleave the bits of X and Y.
        x &= 0xFFFF;
        x = (x | (x << 8)) & 0x00FF00FF;
        x = (x | (x << 4)) & 0x0F0F0F0F;
        x = (x | (x << 2)) & 0x33333333;
        x = (x | (x << 1)) & 0x55555555;

        y &= 0xFFFF;
        y = (y | (y << 8)) & 0x00FF00FF;
        y = (y | (y << 4)) & 0x0F0F0F0F;
        y = (y | (y << 2)) & 0x33333333;
        y = (y | (y << 1)) & 0x55555555;

        var result = x | (y << 1);
        return result;
    }


    return {
        formatted_string_to_html: formatted_string_to_html,
        init_canvas: init_canvas,
        make_key: make_key
    };
});
