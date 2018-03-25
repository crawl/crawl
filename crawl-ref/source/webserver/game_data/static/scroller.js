define(["jquery", "contrib/simplebar"],
function ($, simplebar) {
    "use strict";

    return function (el) {
        var scroller = $(el).data("scroller");
        if (scroller === undefined)
        {
            var $el = $(el);
            scroller = new simplebar(el);
            $el.data("scroller", scroller);

            var scrollElement = scroller.getScrollElement();
            var contentElement = scroller.getContentElement();
            var top = $("<div class='scroller-shade top'>"),
                bot = $("<div class='scroller-shade bot'>");
            $el.append(top).append(bot);
            var update_shades = function () {
                var dy = $(scrollElement).scrollTop(),
                    ch = $(contentElement).outerHeight(),
                    sh = $(scrollElement).outerHeight();
                top.css("opacity", dy/100/4);
                bot.css("opacity", (ch - sh - dy)/100/4);
            };
            scrollElement.addEventListener("scroll", update_shades);
            update_shades();
        }
        return {
            contentElement: scroller.getContentElement(),
            scrollElement: scroller.getScrollElement(),
        };
    };
});
