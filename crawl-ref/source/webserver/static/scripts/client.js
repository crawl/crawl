define(["exports", "jquery", "react", "comm",
        "jsx!lobby", "jsx!loader", "pubsub", "keyboard"],
function (exports, $, React, comm, LobbyRoot, Loader, pubsub) {
    "use strict";

    window.debug_mode = false;

    // Root component for the whole client
    var PageRoot = React.createClass({
        displayName: "PageRoot",
        getDefaultProps: function () {
            return {
                state: "connecting"
            };
        },
        render: function () {
            switch (this.props.state)
            {
            case "connecting":
                return React.DOM.div({}, "Connecting...");
            case "connect_failure":
                return React.DOM.div({}, "Could not connect to server.");
            case "lobby":
                return LobbyRoot();
            case "play":
            case "watch":
                return Loader();
            case "closed":
                return React.DOM.div({}, "The connection was closed:",
                                     React.DOM.br(),
                                     this.props.msg);
            default:
                return React.DOM.div({}, "BUG: Unknown state ", this.props.state);
            }
        }
    });

    handle_old_hash();

    // Render the page
    var root = React.renderComponent(PageRoot(),
                                     document.getElementById("root"));

    comm.register_immediate_handlers({
        "ping": pong,
        "close": connection_closed,
    });

    comm.register_handlers({
        "html": set_html,
        "go": go_to
    });

    comm.connect(connect_success, connect_failure);

    function update(state)
    {
        root.setProps(state);
    }

    Object.defineProperty(exports, "state", {
        get: function () {
            return root.props.state;
        }
    });

    function handle_old_hash()
    {
        var play = location.hash.match(/^#play-(.+)/);
        if (play)
        {
            var game_id = play[1];
            if (history)
                history.replaceState(null, "", "/play/" + game_id);
            else
                location = "/play/" + game_id;
        }
        var watch = location.hash.match(/^#watch-(.+)/);
        if (watch)
        {
            var watch_user = watch[1];
            if (history)
                history.replaceState(null, "", "/watch/" + watch_user);
            else
                location = "/watch/" + watch_user;
        }
    }

    function do_url_action()
    {
        var play = location.pathname.match(/^\/play\/(.+)/);
        if (play)
        {
            var game_id = play[1];
            comm.send_message("play", {
                game_id: game_id
            });
            update({state: "play"});
        }
        var watch = location.pathname.match(/^\/watch\/(.+)/);
        if (watch)
        {
            var watch_user = watch[1];
            comm.send_message("watch", {
                username: watch_user
            });
            update({state: "watch"});
        }
        else if (location.pathname == "/")
        {
            comm.send_message("lobby");
            update({state: "lobby"});
        }
    }

    pubsub.subscribe("url_change", do_url_action);

    window.onpopstate = function (ev) {
        location.reload();
    };

    function connect_success()
    {
        do_url_action(false);
    }

    function connect_failure()
    {
        update({state: "connect_failure"});
    }

    // Message handlers -----------------------------------
    function pong(data)
    {
        comm.send_message("pong");
        return true;
    }

    function connection_closed(data)
    {
        if (data.reconnect)
            location.reload();
        else
            update({state: "closed", msg: data.reason});
        return true;
    }

    function go_to(data)
    {
        document.location = data.path;
    }

    function set_html(data)
    {
        console.warn("Received 'html' message!", data);
        $("#" + data.id).html(data.content);
    }

    // Dialog handling ------------------------------------
    function show_dialog(id)
    {
        $(".floating_dialog").hide();
        var elem = $(id);
        elem.stop(true, true).fadeIn(100, function () {
            elem.focus();
        });
        center_element(elem);
        $("#overlay").show();
    }
    function center_element(elem)
    {
        var left = $(window).width() / 2 - elem.outerWidth() / 2;
        if (left < 0) left = 0;
        var top = $(window).height() / 2 - elem.outerHeight() / 2;
        if (top < 0) top = 0;
        if (top > 50) top = 50;
        var offset = elem.offset();
        if (offset.left != left || offset.top != top)
            elem.offset({ left: left, top: top });
    }
    function hide_dialog()
    {
        $(".floating_dialog").blur().hide();
        $("#overlay").hide();
    }
    exports.show_dialog = show_dialog;
    exports.hide_dialog = hide_dialog;
    exports.center_element = center_element;

    exports.is_watching = function () {
        return root.props.state === "watch";
    };

    window.assert = function () {};
    window.log = function (text)
    {
        if (window.console && window.console.log)
            window.console.log(text);
    }

    return exports;
});
