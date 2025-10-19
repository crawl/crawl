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
        // this is a bit weird because the crawl binary never closes color
        // tags in auto-generated format_string text (which is a valid color
        // string), but we don't want the crazy arbitrary nesting of spans
        // this can lead to. So it ensures that only one fg and one bg span
        // are involved at any time.
        var cur_fg = [];
        var bg_open = false; // XX nesting bg tags not handled
        var filtered = str.replace(/<?<(\/?(bg:)?[a-z]*)>?|>|&/ig, function (str, p1) {
            if (p1 === undefined)
                p1 = "";
            var closing = false;
            var bg = false;
            if (p1.match(/^\//))
            {
                p1 = p1.substr(1);
                closing = true;
            }
            if (p1.match(/^bg:/))
            {
                bg = true;
                p1 = p1.substr(3);
            }
            if (p1 in cols && !str.match(/^<</) && str.match(/>$/))
            {
                if (closing)
                {
                    if (bg && bg_open)
                    {
                        bg_open = false;
                        return "</span>";
                    }
                    else if (cur_fg.length > 0)
                    {
                        cur_fg.pop();
                        if (cur_fg.length > 0)
                        {
                            // restart the previous color
                            return "</span><span class='fg"
                                        + cur_fg[cur_fg.length - 1] + "'>";
                        }
                        else
                            return "</span>";
                    }
                    // mismatched close tag
                    return "";
                }
                else if (bg)
                {
                    var text = "<span class='bg" + cols[p1] + "'>";
                    if (bg_open)
                        text = "</span>" + text;
                    // if a fg span is currently open, close it before the
                    // new background span, and reopen it inside that span.
                    // This ensures that fg spans are always nested inside
                    // bg spans.
                    if (cur_fg.length > 0)
                    {
                        text = "</span>" + text
                                + "<span class='fg"
                                + cur_fg[cur_fg.length - 1] + "'>";
                    }
                    bg_open = true;
                    return text;
                }
                else
                {
                    var text = "<span class='fg" + cols[p1] + "'>"
                    // close out a currently open fg span, to keep only one
                    // at a time
                    if (cur_fg.length > 0)
                        text = "</span>" + text;
                    cur_fg.push(cols[p1]);
                    return text;
                }
            }
            else
            {
                if (str.match(/^<</))
                    return escape_html(str.substr(1));
                else
                    return escape_html(str);
            }
        });
        if (cur_fg.length > 0)
            filtered += "</span>";
        if (bg_open)
            filtered += "</span>";
        // TODO: eliminate empty spans?
        return filtered;
    }

    function init_canvas(element, w, h) {
        // in an ideal world, we might be able to just apply a scale by ratio
        // here and be done with it. However, fractional pixel snapping is
        // very dicey and varied across browsers. So the scale by ratio will
        // mostly need to be manually applied when a canvas created by this
        // function is used. Things might be simpler if this value consistently
        // represented just the device vs logical pixel ratio, but aside from
        // safari, it also factors in browser zoom.
        const ratio = window.devicePixelRatio;
        element.width = Math.floor(w * ratio);
        element.height = Math.floor(h * ratio);
        // var ctx = element.getContext('2d');
        // ctx.resetTransform();
        // ctx.scale(ratio, ratio);
        element.style.width = w + 'px';
        element.style.height = h + 'px';
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
