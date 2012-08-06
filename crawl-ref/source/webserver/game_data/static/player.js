define(["jquery", "comm"],
function ($, comm) {
    var you = {};

    function handle_player_message(data)
    {
        log(data);
        delete data.msg;

        $.extend(you, data);
    }

    comm.register_handlers({
        "player": handle_player_message,
    });

    return you;
});
