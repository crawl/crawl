/** @jsx React.DOM */
define(["react", "comm", "pubsub", "user", "jsx!misc", "jsx!login", "jquery"],
function (React, comm, pubsub, user, misc, login, $) {
    "use strict";

    var extend = $.extend;

    var Overlay = misc.Overlay;
    var LoginForm = login.LoginForm;

    // RC file editor form
    var RcEditor = React.createClass({
        getInitialState: function () {
            return {
                contents: null
            };
        },
        componentDidMount: function () {
            comm.register_handler("rcfile_contents", this.receive_rc_contents);
            comm.send_message("get_rc", { game_id: this.props.game_id });
        },
        componentWillUnmount: function () {
            comm.unregister_handler("rcfile_contents", this.receive_rc_contents);
        },

        receive_rc_contents: function (data) {
            this.setState({contents: data.contents});
            this.refs.editor.getDOMNode().focus();
        },

        handle_change: function (ev) {
            this.setState({contents: ev.target.value});
        },
        handle_cancel: function (ev) {
            this.props.on_finished();
        },
        handle_save: function (ev) {
            comm.send_message("set_rc", {
                game_id: this.props.game_id,
                contents: this.state.contents
            });
            this.props.on_finished();
        },

        render: function () {
            var textbox_style = {
                boxSizing: "border-box",
                margin: 0,
                width: "100%"
            };
            return <div style={{overflow: "hidden"}}>
                     <h3>Edit RC</h3>
                     <textarea value={this.state.contents}
                               onChange={this.handle_change}
                               style={textbox_style}
                               disabled={this.state.contents === null}
                               cols="80" rows="25" ref="editor" />
                     <br />
                     <input type="button" value="Cancel"
                            onClick={this.handle_cancel} />
                     <input type="submit" value="Save"
                            style={{"float": "right"}}
                            onClick={this.handle_save} />
                     <br />
                   </div>;
        }
    });

    // Game selector
    //
    // Shows saved games and available game versions, and handles RC
    // file editing.
    var GameSelector = React.createClass({
        getInitialState: function () {
            return {
                selected_game: this.props.games[0],
                editing_rc: null
            };
        },

        load_game: function (g) {
            if (history)
            {
                history.pushState(null, "", "/play/" + g.id);
                pubsub.publish("url_change");
            }
            else
                location.href = "/play/" + g.id;
        },
        start_game: function (ev) {
            if (history)
            {
                ev.preventDefault();
                history.pushState(null, "", ev.target.href);
                pubsub.publish("url_change");
            }
        },
        edit_rc: function (ev) {
            var game_id = this.state.selected_game.id;
            this.setState({editing_rc: game_id});
            ev.preventDefault();
        },
        stop_editing: function () {
            this.setState({editing_rc: null});
        },
        select_version: function (v) {
            console.log("select", v);
            var game;
            if (v !== this.more_versions_text())
                game = collect_version_games(this.props.games, v)[0];
            else
            {
                game = this.props.games.filter(function (g) {
                    return this.is_hidden_version(g.version || g.name);
                }.bind(this))[0];
            }
            this.setState({selected_game: game});
        },
        select_game: function (g) {
            console.log("select", g);
            this.setState({selected_game: g});
        },

        is_hidden_version: function (v) {
            var hide_versions = this.props.settings.hide_versions;
            if (hide_versions)
                return hide_versions.indexOf(v) >= 0;
            else
                return false;
        },
        more_versions_text: function () {
            return this.props.settings.more_versions || "More...";
        },

        render: function () {
            // Saved games
            var saved_games = this.props.saved_games;
            var saved = saved_games.length === 0 ? null : (
              <div className="saved_games">Saved games:
                <ol>
                  {saved_games.map(function (g) { return (
                    <li onClick={this.load_game.bind(this, g)}>
                      {g.name}, a level {g.lvl} {g.race}
                      {g.god ? " of " + g.god : null}
                      {" "} ({g.game.name})
                    </li>
                  ); }.bind(this))}
                </ol>
              </div>
            );

            // Version selector
            var sel = this.state.selected_game;
            var sel_ver = sel.version || sel.name;
            // show all hidden versions as long as one is selected
            var show_hidden = this.is_hidden_version(sel_ver);
            var versions = collect_versions(this.props.games);
            if (!show_hidden)
            {
                var total_length = versions.length;
                versions = versions.filter(function (v) {
                    return !this.is_hidden_version(v);
                }.bind(this));
                // add "More..." button
                if (versions.length < total_length)
                    versions.push(this.more_versions_text());
            }
            var version_select = (
              <div className="version_select">Version:
                <ol className="select">
                  {versions.map(function (v) { return (
                    <li onClick={this.select_version.bind(this, v)} key={v}
                        className={sel_ver === v ? "selected" : ""}>{v}</li>
                  ); }.bind(this))}
                </ol>
              </div>
            );
            var games = collect_version_games(this.props.games, sel_ver);
            var mode_select = games.length <= 1 ? null : (
              <div>Mode:
                <ol className="select">
                  {games.map(function (g) { return (
                    <li onClick={this.select_game.bind(this, g)}
                        key={g.mode || g.name}
                        className={sel === g ? "selected" : ""}>
                      {g.mode || g.name}
                    </li>
                  ); }.bind(this))}
                </ol>
              </div>
            );
            var comment = sel.comment ? (<div>{sel.comment}</div>) : null;
            var golink = (
              <a className="start_game" href={"/play/" + sel.id}
                 onClick={this.start_game}>
                {"Start " + sel.name}
              </a>
            );
            var editlink = (
              <a className="edit_rc" href="javascript:" onClick={this.edit_rc}>
                edit the settings file
              </a>
            );
            var select = (
              <div>
                {golink}, {editlink} or select a different version:
                {version_select}
                {mode_select}
                {comment}
              </div>
            );

            var rc = null;
            if (this.state.editing_rc)
            {
                rc = (
                  <Overlay on_cancel={this.stop_editing}>
                    <RcEditor game_id={this.state.editing_rc}
                              on_finished={this.stop_editing} />
                  </Overlay>
                );
            }
            return <div>{saved}{select}{rc}</div>;
        }
    });

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
        var modes = [];
        return games.filter(function (g) {
            return (g.version || g.name) === v;
        });
    }


    var LogoutLink = React.createClass({
        handle_click: function (ev) {
            user.logout();
            ev.preventDefault();
        },
        render: function () {
            return <a onClick={this.handle_click} href="javascript:">Logout</a>;
        }
    });

    // One line in the list of running games
    var RunningGameEntry = React.createClass({
        handle_click: function (ev) {
            if (history)
            {
                ev.preventDefault();
                history.pushState(null, "", ev.target.href);
                pubsub.publish("url_change");
            }
        },

        render: function () {
            function format_time(seconds)
            {
                if (seconds == 0)
                    return "";
                else if (seconds < 120)
                    return Math.round(seconds) + " s";
                else if (seconds < (60 * 60))
                    return Math.round(seconds / 60) + " min";
                else
                    return Math.round(seconds / (60 * 60)) + " h";
            }
            var g = this.props.game;
            var idle_time;
            if (g.idle_time > 0)
                idle_time = format_time((new Date() - g.idle_start)/1000);
            else
                idle_time = null;
            var watch = <a href={"/watch/" + g.username}
                           onClick={this.handle_click}>{g.username}</a>;
            return <tr>
                     <td>{watch}</td>
                     <td className="game_id">{g.game_id}</td>
                     <td className="xl">{g.xl}</td>
                     <td className="char">{g.char}</td>
                     <td className="place">{g.place}</td>
                     <td className="god">{g.god}</td>
                     <td className="idle_time">{idle_time}</td>
                     <td className="spectator_count">{g.spectator_count}</td>
                     <td className="milestone">{g.milestone}</td>
                   </tr>;
        }
    });

    // The list of running games
    //
    // Mostly handles sorting (this could be split into a generic component)
    var RunningGamesList = React.createClass({
        getInitialState: function () {
            return {
                sort: [{field: "username", down: false}]
            };
        },
        componentDidMount: function () {
            this.interval = setInterval(this.forceUpdate.bind(this), 1000);
        },
        componentWillUnmount: function () {
            clearInterval(this.interval);
        },

        header_click: function (ev) {
            var field = ev.target.getAttribute("data-field");
            sort = this.state.sort;
            if (sort[0].field === field)
                sort[0].down = !sort[0].down;
            else
            {
                sort = sort.filter(function (s) {
                    return s.field !== field;
                });
                sort.unshift({field: field, down: false});
            }
            this.setState({sort: sort});
        },

        render: function () {
            var data = this.props.games.slice(0), sort = this.state.sort;
            function compare_entries(a, b)
            {
                for (var i = 0; i < sort.length; ++i)
                {
                    var f = sort[i].field;
                    var x = a[f] || "", y = b[f] || "";
                    if (x !== y)
                    {
                        var result;
                        if (f === "xl")
                        {
                            x = parseInt(x) || -1;
                            y = parseInt(y) || -1;
                            result = x - y;
                        }
                        else if (f === "place")
                        {
                            var parts_a = x.split(":");
                            var parts_b = y.split(":");
                            if (parts_a[0] !== parts_b[0])
                                result = parts_a[0] < parts_b[0] ? -1 : 1;
                            else
                                result = parseInt(parts_a[1]) - parseInt(parts_b[1]);
                        }
                        else
                            result = x < y ? -1 : 1;
                        if (sort[i].down)
                            result *= -1;
                        return result;
                    }
                }
                return 0;
            }
            data.sort(compare_entries);
            function make_entry(game)
            {
                return <RunningGameEntry game={game} key={game.id} />;
            }
            var main_sort = sort[0].field;
            var sort_class = sort[0].down ? "headerSortDown" : "headerSortUp";
            function h(field, content)
            {
                var cls = field;
                if (field === main_sort)
                    cls += " " + sort_class;
                return <th className={cls} data-field={field}>{content}</th>;
            }
            var t = <table className="player_list">
                      <thead>
                        <tr onClick={this.header_click}>
                          {h("username", "User")}
                          {h("game_id", "Game")}
                          {h("xl", "XL")}
                          {h("char", "Char")}
                          {h("place", "Place")}
                          {h("god", "God")}
                          {h("idle_time", "Idle")}
                          {h("spectator_count", "Specs")}
                          {h("milestone", "Milestone")}
                        </tr>
                      </thead>
                      <tbody>
                        {data.map(make_entry)}
                      </tbody>
                    </table>;
            return <div><span>Games currently running:</span>{t}</div>;
        }
    });

    // Root component for the lobby
    //
    // Handles lobby events and stores the relevant data as state.
    var LobbyRoot = React.createClass({
        getInitialState: function () {
            return {
                username: null,
                games: null,
                game_link_settings: null,
                saved_games: null,
                running_games: [],
                banner: null,
                footer: null
            };
        },
        componentDidMount: function () {
            comm.register_handler("lobby", this.lobby);
            comm.register_handler("lobby_html", this.lobby_html);
            comm.register_handler("game_info", this.game_info);
        },
        componentWillUnmount: function () {
            comm.unregister_handler("lobby", this.lobby);
            comm.unregister_handler("lobby_html", this.lobby_html);
            comm.unregister_handler("game_info", this.game_info);
        },

        lobby: function (data) {
            var rg = this.state.running_games;
            if (data.remove)
            {
                for (var i = 0; i < rg.length; ++i)
                {
                    if (rg[i].id === data.remove)
                    {
                        rg.splice(i, 1);
                        break;
                    }
                }
            }
            if (data.entries)
            {
                for (var j = 0; j < data.entries.length; ++j)
                {
                    var d = new Date();
                    d.setSeconds(d.getSeconds() - data.entries[j].idle_time);
                    data.entries[j].idle_start = d;
                    var found = false;
                    for (var i = 0; i < rg.length; ++i)
                    {
                        if (rg[i].id === data.entries[j].id)
                        {
                            extend(rg[i], data.entries[j]);
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        rg.push(data.entries[j]);
                }
            }
            this.setState({running_games: rg});
        },
        lobby_html: function (data) {
            this.setState({
                banner: data.banner,
                footer: data.footer
            });
        },
        game_info: function (data) {
            this.setState({games: data.games,
                           game_link_settings: data.settings || {},
                           saved_games: data.saved_games});
        },

        logged_in: function (username) {
            this.setState({username: username});
        },

        render: function () {
            var login_line;
            if (this.state.username)
            {
                login_line = <div style={{float: "right",
                                          textAlign: "right"}}
                                  className="login">
                               <LogoutLink />
                             </div>;
            }
            else
            {
                login_line = <div style={{float: "right",
                                          textAlign: "right"}}
                                  className="login">
                               <LoginForm on_login={this.logged_in} />
                             </div>;
            }
            var links = null;
            if (this.state.games)
                links = <GameSelector games={this.state.games}
                                      settings={this.state.game_link_settings}
                                      saved_games={this.state.saved_games} />;
            var running_games = null;
            if (this.state.running_games.length)
                running_games = <RunningGamesList games={this.state.running_games} />;
            var banner = <header dangerouslySetInnerHTML={{__html: this.state.banner}} />;
            var footer = <footer dangerouslySetInnerHTML={{__html: this.state.footer}} />;
            return <div className="lobby">
                     {login_line}
                     {banner}
                     {links}
                     {running_games}
                     {footer}
                   </div>;
        }
    });

    return LobbyRoot;
});
