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

    function register_message_handlers(dict)
    {
        $.extend(message_handlers, dict);
    }

    function handle_message(msg)
    {
        var msgobj;
        if (typeof msg === "string")
            msgobj = eval("(" + msg + ")");
        else
            msgobj = msg;
        var handler = message_handlers[msgobj.msg];
        if (!handler)
        {
            console.error("Unknown message type: " + msgobj.msg);
            return;
        }
        handler(msgobj);
    }

    return {
        send_message: send_message,
        register_handlers: register_message_handlers,
        handle_message: handle_message,
    };
});
