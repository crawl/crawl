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
                    return "</span>";
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

    return {
        formatted_string_to_html: formatted_string_to_html,
        init_canvas: init_canvas
    };
});
