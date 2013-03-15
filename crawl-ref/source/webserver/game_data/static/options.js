define(["jquery", "comm"],
function ($, comm) {
    var options = null;

    function handle_options_message(data)
    {
        if (options != null)
            return;
        options = data.options;
        log(options);
    }

    comm.register_handlers({
        "options": handle_options_message,
    });

    return options;
});

