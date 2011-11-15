define(["jquery", "contrib/jquery.json"], function ($) {
    window.socket = null;

    function send_message(msg, data)
    {
        data = data || {};
        data["msg"] = msg;
        socket.send($.toJSON(data));
    }

    // JSON message handlers
    var message_handlers = {};
    var immediate_handlers = {};

    function register_message_handlers(dict)
    {
        $.extend(message_handlers, dict);
    }

    function register_immediate_handlers(dict)
    {
        $.extend(immediate_handlers, dict);
    }

    function handle_message(msg)
    {
        var handler = message_handlers[msg.msg];
        if (!handler)
        {
            console.error("Unknown message type: " + msg.msg);
            return;
        }
        handler(msg);
    }

    function handle_message_immediately(msg)
    {
        var handler = immediate_handlers[msg.msg];
        if (handler)
        {
            return handler(msg);
        }
        else
            return false;
    }

    return {
        send_message: send_message,
        register_handlers: register_message_handlers,
        register_immediate_handlers: register_immediate_handlers,
        handle_message: handle_message,
        handle_message_immediately: handle_message_immediately,
    };
});
