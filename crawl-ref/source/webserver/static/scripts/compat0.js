define(["jquery", "comm", "client", "contrib/jquery.waitforimages"],
function($, comm, client) {
    delete window.game_loading;

    window.current_layer = "crt";

    var layers = ["crt", "normal", "lobby", "loader"];

    function set_layer(layer)
    {
        client.hide_prompt();

        $.each(layers, function (i, l) {
            if (l == layer)
                $("#" + l).show();
            else
                $("#" + l).hide();
        });
        current_layer = layer;

        $("#chat").toggle(client.in_game());
    }

    function register_layer(name)
    {
        if (layers.indexOf(name) == -1)
            layers.push(name);
    }

    client.set_layer = set_layer;
    client.register_layer = register_layer;

    function do_set_layer(data)
    {
        set_layer(data.layer);
    }

    // Global functions
    window.set_layer = set_layer;
    window.abs = function (x) { if (x < 0) return -x; else return x; }

    comm.register_handlers({
        "layer": do_set_layer
    });

    return function (content) {
        if (content.indexOf("game_loading") === -1)
        {
            // old version, uninhibit can happen too early
            $("#game").waitForImages(client.uninhibit_messages);
        }
        else
        {
            $("#game").waitForImages(function () {
                var load_interval = setInterval(check_loading, 50);

                function check_loading()
                {
                    if (window.game_loading)
                    {
                        delete window.game_loading;
                        client.uninhibit_messages();
                        clearInterval(load_interval);
                    }
                }
            });
        }
    };
});
