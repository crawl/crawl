define([], function () {
    var channels = {};

    function publish()
    {
        var args = arguments;
        function doit()
        {
            publish_sync.apply(null, args);
        }
        setTimeout(doit, 0);
    }

    function publish_sync(channel)
    {
        var args = [];
        if (arguments.length > 1)
            args = Array.prototype.splice.call(arguments, 1);

        var subs = channels[channel];
        if (subs)
        {
            for (var i = 0; i < subs.length; ++i)
            {
                subs[i].apply(null, args);
            }
        }
    }

    function subscribe(channel, handler)
    {
        if (!channels[channel])
            channels[channel] = [];
        channels[channel].push(handler);
    }

    function unsubscribe(channel, handler)
    {
        var subs = channels[channel];
        for (var i = 0; i < subs.length; ++i)
        {
            if (subs[i] === handler)
            {
                subs.splice(i, 1);
                break;
            }
        }
    }

    return {
        publish: publish,
        publish_sync: publish_sync,
        subscribe: subscribe,
        unsubscribe: unsubscribe
    };
});
