define(["jquery", "comm"],
function ($, comm) {
    "use strict";

    var options = null;
    var listeners = $.Callbacks();

    function clear_options()
    {
        options = null;
    }

    function get_option(name)
    {
        if (options == null)
        {
            console.error("Options not set, wanted option: " + name);
            return null;
        }

        if (!options.hasOwnProperty(name))
        {
            console.error("Option doesn't exist: " + name);
            return null;
        }

        return options[name];
    }

    function pwa_minimum_option(name, value)
    {
        var pwa = window.DCSS_PWA;
        if (!pwa || !pwa.enabled || !pwa.optionMinimums
            || !pwa.optionMinimums.hasOwnProperty(name))
        {
            return value;
        }

        var minimum = pwa.optionMinimums[name];
        if (typeof value === "number" && value < minimum)
            return minimum;

        return value;
    }

    function apply_pwa_minimums(values)
    {
        if (!values)
            return;

        for (var name in values)
            if (values.hasOwnProperty(name))
                values[name] = pwa_minimum_option(name, values[name]);
    }

    // writes all options at once: usable only on first connecting.
    function handle_options_message(data)
    {
        options = data.options;
        apply_pwa_minimums(options);
        listeners.fire();
    }

    function add_listener(callback)
    {
        listeners.add(callback);
    }

    function set_option(name, value, fire_listeners=true)
    {
        var old = get_option(name);
        options[name] = pwa_minimum_option(name, value);
        if (fire_listeners)
            listeners.fire();
        return old != options[name];
    }

    function send_option(name)
    {
        var value = get_option(name);
        if (value !== null && value !== undefined)
            comm.send_message("set_option", {"line": name + " = " + value });
    }

    function handle_set_option(data)
    {
        set_option(data.name, data.value);
    };

    window.set_option = set_option;

    comm.register_handlers({
        "options": handle_options_message,
        "set_option": handle_set_option
    });

    return {
        get: get_option,
        set: set_option,
        send: send_option,
        clear: clear_options,
        add_listener: add_listener,
    };
});
