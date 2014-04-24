define(["comm", "pubsub"], function (comm, pubsub) {
    var username = null;

    function logged_in(data)
    {
        username = data.username;

        var cookie = "sid=" + data.sid + "; path=/";
        if (location.protocol === "https:")
            cookie += "; secure";

        document.cookie = cookie;

        pubsub.publish("logged_in", username);
    }

    function utc_format(ts)
    {
        // convert UTC timestamp (seconds) to RFC-1123 string
        var d = new Date();
        d.setTime(ts * 1000 - d.getTimezoneOffset()*60000);
        return d.toUTCString();
    }

    function login_cookie(data)
    {
        var cookie = "login=" + data.cookie + "; path=/";
        cookie += "; expires=" + utc_format(data.expires);
        if (location.protocol === "https:")
            cookie += "; secure";
        document.cookie = cookie;
    }

    function get_cookie(key)
    {
        return decodeURIComponent(document.cookie.replace(new RegExp("(?:(?:^|.*;)\\s*" + encodeURIComponent(key).replace(/[\-\.\+\*]/g, "\\$&") + "\\s*\\=\\s*([^;]*).*$)|^.*$"), "$1")) || null;
    }

    function login(un, pw, rememberme)
    {
        comm.send_message("login", {
            username: un,
            password: pw,
            rememberme: rememberme
        });
    }

    function logout()
    {
        username = null;
        comm.send_message("logout", {
            cookie: get_cookie("login")
        });

        var cookie = "login=; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT";
        if (location.protocol === "https:")
            cookie += "; secure";
        document.cookie = cookie;

        cookie = "sid=; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT";
        if (location.protocol === "https:")
            cookie += "; secure";
        document.cookie = cookie;

        location.reload();
    }

    comm.register_handlers({
        "login_success": logged_in,
        "login_cookie": login_cookie
    });

    return {
        get username() {
            return username;
        },
        login: login,
        logout: logout
    };
});
