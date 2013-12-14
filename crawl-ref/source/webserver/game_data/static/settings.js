define(["jquery", "client", "contrib/jquery.cookie", "contrib/jquery.json"],
function ($, client) {
    "use strict";

    var settings = {};
    var settings_cookie = "settings";

    function get(key)
    {
        return settings[key];
    }

    function set_defaults(map)
    {
        for (var key in map)
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

        save_to_cookie();

        $(document).trigger("settings_changed", [map]);
    }

    function save_to_cookie()
    {
        $.cookie(settings_cookie, $.toJSON(settings));
    }

    function load_from_cookie()
    {
        var cookie = $.cookie(settings_cookie);
        if (!cookie) return;
        var val = $.secureEvalJSON(cookie);
        set(val);
    }

    function show_dialog()
    {
        $("#settings input").each(function () {
            var value = get($(this).data("setting"));
            if ($(this).attr("type") == "checkbox")
            {
                $(this).attr("checked", value);
            }
            else if ($(this).attr("type") == "radio")
            {
                $(this).val([value]);
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
        $("#settings :focus").blur();
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

        load_from_cookie();
    });

    return {
        get: get,
        set: set,
        set_defaults: set_defaults,
    };
});
