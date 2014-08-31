define(["comm", "pubsub"], function (comm, pubsub) {
    var username = null;

    function nerd_description(name, nerdtype, devname)
    {
        if (nerdtype == "normal")
            return "";

        var desc = {"admins" : "a Server Administrator",
                    "devteam" : "a DCSS Developer",
                    "goodplayers" : "a goodplayer (10+ wins)",
                    "greatplayers" : "a greatplayer (has won every species)",
                    "greaterplayers" : "a greaterplayer (has won every species " +
                                      "and background)",
                    "centuryplayers" : "a centuryplayer (100+ wins)"};

        var fullname = name;
        if (nerdtype == "devteam" && name !== devname)
            fullname = fullname + " (" + devname + ")";
        return fullname + " is " + desc[nerdtype];
    }

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

    function collect_versions(games)
    {
        var versions = [];
        for (var i = 0; i < games.length; ++i)
        {
            if (versions.indexOf(games[i].version || games[i].name) === -1)
                versions.push(games[i].version || games[i].name);
        }
        return versions;
    }

    function collect_version_games(games, v)
    {
        return games.filter(function (g) {
            return (g.version || g.name) === v;
        });
    }

    function collect_maps(games, version)
    {
        var sprint_maps = [];
        var have_crawl = false;
        var have_zotdef = false;

        for (var i = 0; i < games.length; ++i)
        {
            if ((games[i].version || games[i].name) !== version)
                continue;
            if (games[i].mode === "Dungeon Crawl")
                have_crawl = true;
            else if (games[i].mode === "Zot Defence")
                have_zotdef = true;
            else if (games[i].mode === "Dungeon Sprint"
                     && games[i].hasOwnProperty("game_maps"))
            {
                for (var j = 0; j < games[i].game_maps.length; ++j)
                {
                    var m = games[i].game_maps[j];
                    sprint_maps.push({game_mode : "sprint", map_name : m.name,
                                      map_desc : m.description});
                }
            }
        }

        var map_list = [];
        if (have_crawl)
            map_list.push({game_mode : "dcss", map_name : "dcss",
                           map_desc : "Dungeon Crawl"});
        map_list = map_list.concat(sprint_maps);
        if (have_zotdef)
        {
            map_list.push({game_mode : "zotdef", map_name : "zotdef",
                           map_desc : "Zot Defence"});
        }
        return map_list;
    }

    function full_mode_name(mode)
    {
        var mode_names = {dcss : "Dungeon Crawl",
                          sprint : "Dungeon Sprint",
                          zotdef : "Zot Defence"};
        if (mode_names.hasOwnProperty(mode))
            return mode_names[mode];
        else
            return null;
    }

    function abbrev_mode_name(mode)
    {
        var mode_names = {"Dungeon Crawl" : "dcss",
                          "Dungeon Sprint" : "sprint",
                          "Zot Defence" : "zotdef"};
        if (mode_names.hasOwnProperty(mode))
            return mode_names[mode];
        else
            return null;
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
        logout: logout,
        nerd_description: nerd_description,
        collect_versions: collect_versions,
        collect_version_games: collect_version_games,
        collect_maps: collect_maps,
        full_mode_name: full_mode_name,
        abbrev_mode_name: abbrev_mode_name,
    };
});
