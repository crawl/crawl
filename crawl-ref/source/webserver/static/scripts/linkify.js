define(["jquery", "contrib/ba-linkify.min"], function ($) {
    "use strict";

    var ALLOWED_PROTOCOLS = ["http", "https", "ftp", "irc"];

    var ba_linkify = window.linkify;
    delete window.linkify;

    function linkify(text)
    {
        return ba_linkify(text,
        {
            callback: function (text, href)
            {
                if (!href)
                    return text;
                if (!ALLOWED_PROTOCOLS.some(function (p) {
                        return href.indexOf(p+ "://") === 0; }))
                {
                    return text;
                }
                return $("<a>").attr("href", href).attr("target", "_blank")
                    .text(text)[0].outerHTML;
            }
        });
    }

    return linkify;
});
