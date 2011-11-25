define(["jquery", "client"], function ($, client) {
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

    function show_dialog()
    {
        $("#settings input").each(function () {
            var value = get($(this).data("setting"));
            if ($(this).attr("type") == "checkbox")
            {
                $(this).attr("checked", value);
            }
            else
            {
                $(this).val(value);
            }
        });
        client.show_dialog("#settings");
        $("#settings input").first().focus();
    }

    function settings_keydown_handler(ev)
    {
        if ($("#settings:visible").length
            && (ev.which == 27 || ev.which == 121))
        {
            close_dialog();
            return false;
        }
        else if ($(".floating_dialog:visible").length == 0 && ev.which == 121)
        {
            show_dialog();
            return false;
        }
    }

    function close_dialog()
    {
        client.hide_dialog("#settings");
        $(document).focus();
    }

    function dialog_element_changed(ev)
    {
        var value;
        if ($(this).attr("type") == "checkbox")
        {
            value = !!$(this).attr("checked");
        }
        else
        {
            value = $(this).val();
        }
        set($(this).data("setting"), value);
    }

    $(document).off("game_init.settings");
    $(document).on("game_init.settings", function () {
        $(document).off("game_keydown.settings");
        $(document).on("game_keydown.settings", settings_keydown_handler);
        $("#settings").on("change", "input", dialog_element_changed);
        $("#close_settings").click(close_dialog);
    });

    return {
        get: get,
        set: set,
        set_defaults: set_defaults,
    };
});
