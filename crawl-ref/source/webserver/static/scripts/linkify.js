define(["react", "contrib/ba-linkify"], function (React) {
    "use strict";

    var ALLOWED_PROTOCOLS = /^http:|^https:|^ftp:|^irc:/;

    var ba_linkify = window.linkify;
    delete window.linkify;

    function react_linkify(text)
    {
        var parts = [];
        ba_linkify(text, {callback: callback});
        function callback(text, href)
        {
            if (href == null)
                parts.push(text);
            else if (href.match(ALLOWED_PROTOCOLS))
                parts.push(React.DOM.a({target: "_blank", href: href}, text));
            else
                parts.push(text);
        }
        return parts;
    }

    return react_linkify;
});
