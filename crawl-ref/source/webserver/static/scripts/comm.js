define(["jquery", "contrib/inflate"], function ($) {
    "use strict";

    window.log_messages = false;
    window.log_message_size = false;

    var socket = null;
    var message_inhibit = 0;
    var message_queue = [];

    function send_message(msg, data)
    {
        data = data || {};
        data["msg"] = msg;
        socket.send(JSON.stringify(data));
    }

    function send_raw(msg)
    {
        socket.send(msg);
    }

    // JSON message handlers
    var message_handlers = {};
    var immediate_handlers = {};

    function register_handler(msgtype, handler)
    {
        message_handlers[msgtype] = handler;
    }

    function unregister_handler(msgtype)
    {
        message_handlers[msgtype] = null;
    }

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
        if (typeof msg === "string")
        {
            // Javascript code
            eval(msg);
        }
        else
        {
            var handler = message_handlers[msg.msg];
            if (!handler)
            {
                console.error("Unknown message type: " + msg.msg);
                return;
            }
            handler(msg);
        }
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

    function enqueue_messages(msgtext)
    {
        if (msgtext.match(/^{/))
        {
            // JSON message
            var msgobj;
            try
            {
                // Can't use JSON.parse here, because 0.11 and older send
                // invalid JSON messages
                msgobj = eval("(" + msgtext + ")");
            }
            catch (e)
            {
                console.error("Parsing error:", e);
                console.error("Source message:", msgtext);
                return;
            }
            var msgs = msgobj.msgs;
            if (msgs == null)
                msgs = [ msgobj ];
            for (var i in msgs)
            {
                if (window.log_messages && window.log_messages !== 2)
                    console.log("Message: " + msgs[i].msg, msgs[i]);
                if (!handle_message_immediately(msgs[i]))
                    message_queue.push(msgs[i]);
            }
        }
        else
        {
            // Javascript code
            message_queue.push(msgtext);
        }
        handle_message_backlog();
    }

    function handle_message_backlog()
    {
        while (message_queue.length
               && (message_inhibit == 0))
        {
            var msg = message_queue.shift();
            if (window.debug_mode)
            {
                handle_message(msg);
            }
            else
            {
                try
                {
                    handle_message(msg);
                }
                catch (err)
                {
                    console.error("Error in message:", msg);
                    console.error(err.message);
                    console.error(err.stack);
                }
            }
        }
    }

    function inhibit_messages()
    {
        message_inhibit++;
    }
    function uninhibit_messages()
    {
        if (message_inhibit > 0)
            message_inhibit--;
        handle_message_backlog();
    }

    function handle_multi_message(data)
    {
        var msg;
        while (msg = data.msgs.pop())
            message_queue.unshift(msg);
    }

    function decode_utf8(bufs, callback)
    {
        var b = new Blob(bufs);
        var f = new FileReader();
        f.onload = function(e) {
            callback(e.target.result)
        }
        f.readAsText(b, "UTF-8");
    }

    var blob_construction_supported = true;
    try {
        var blob = new Blob([new Uint8Array([0])]);
    }
    catch (e)
    {
        blob_construction_supported = false;
        log("Blob construction not supported, disabling compression");
    }

    function inflate_works_on_ua()
    {
        if (!blob_construction_supported)
            return false;
        var b = $.browser;
        if (b.chrome && b.version.match("^14\."))
            return false; // doesn't support binary frames
        if (b.chrome && b.version.match("^19\."))
            return false; // buggy Blob builder
        if (b.safari)
            return false;
        if (b.opera)
            return false; // errors in inflate.js (?)
        return true;
    }

    register_message_handlers({
        "multi": handle_multi_message
    });

    var inflater = null;

    if ("Uint8Array" in window &&
        "Blob" in window &&
        "FileReader" in window &&
        "ArrayBuffer" in window &&
        inflate_works_on_ua())
    {
        inflater = new Inflater();
    }

    function connect(success, failure)
    {
        var WebSocket = null;

        if ("MozWebSocket" in window)
        {
            WebSocket = MozWebSocket;
        }
        else if ("WebSocket" in window)
        {
            WebSocket = window.WebSocket;
        }

        if (WebSocket === null)
        {
            failure();
            return;
        }

        var socket_server;
        if (location.protocol == "http:")
            socket_server = "ws://";
        else
            socket_server = "wss://"
        socket_server += location.host + "/socket";
        if (inflater)
            socket = new WebSocket(socket_server);
        else
            socket = new WebSocket(socket_server, "no-compression");
        socket.binaryType = "arraybuffer";

        socket.onopen = function ()
        {
            success();
        };

        socket.onmessage = function (msg)
        {
            if (inflater && msg.data instanceof ArrayBuffer)
            {
                var data = new Uint8Array(msg.data.byteLength + 4);
                data.set(new Uint8Array(msg.data), 0);
                data.set([0, 0, 255, 255], msg.data.byteLength);
                var decompressed = [inflater.append(data)];
                if (decompressed[0] === -1)
                {
                    console.error("Decompression error!");
                    var x = inflater.append(data);
                }
                decode_utf8(decompressed, function (s) {
                    if (window.log_messages === 2)
                        console.log("Message: " + s);
                    if (window.log_message_size)
                        console.log("Message size: " + s.length);

                    enqueue_messages(s);
                });
                return;
            }

            if (window.log_messages === 2)
                console.log("Message: " + msg.data);
            if (window.log_message_size)
                console.log("Message size: " + msg.data.length);

            enqueue_messages(msg.data);
        };

        socket.onerror = function ()
        {
            console.error("Websocket connection error");
        };

        socket.onclose = function (ev)
        {
            console.log("Websocket connection was closed",
                        ev.code, ev.reason);
        };
    }

    return {
        send_message: send_message,
        send_raw: send_raw,

        register_handler: register_handler,
        unregister_handler: unregister_handler,
        register_handlers: register_message_handlers,
        register_immediate_handlers: register_immediate_handlers,

        inhibit_messages: inhibit_messages,
        uninhibit_messages: uninhibit_messages,

        connect: connect
    };
});
