define(["jquery", "comm"],
function ($, comm) {
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

        if (!name in options)
        {
            console.error("Option doesn't exist: " + name);
            return null;
        }

        return options[name];
    }

    function handle_options_message(data)
    {
        if (options == null || data["watcher"] == true)
        {
            options = data.options;
            listeners.fire();
        }
    }

    function add_listener(callback)
    {
        listeners.add(callback);
    }

    comm.register_handlers({
        "options": handle_options_message,
    });

    return {
        get: get_option,
        clear: clear_options,
        add_listener: add_listener,
    };
});
