define(["jquery"], function ($) {
    var settings = {};

    function get(key)
    {
        return settings[key];
    }

    function set_defaults(map)
    {
        for (key in map)
        {
            if (!(key in settings))
                settings[key] = map[key];
        }
    }

    function set()
    {
        var map;
        if (arguments.length == 2)
        {
            map = {};
            map[arguments[0]] = arguments[1]
        }
        else
            map = arguments[0];

        $.extend(settings, map);

        $(document).trigger("settings_changed", [map]);
    }

    return {
        get: get,
        set: set,
        set_defaults: set_defaults
    };
});
