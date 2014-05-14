/** @jsx React.DOM */
define(["react", "comm", "pubsub", "jsx!misc", "jsx!login", "jsx!chat", "jquery"],
function (React, comm, pubsub, misc, login, Chat, $) {
    "use strict";

    var Overlay = misc.Overlay;
    var LoginForm = login.LoginForm;

    var normal_exit = ["saved", "cancel", "quit", "won", "bailed out", "dead"];

    // Compatibility wrapper for old game HTML
    //
    // Also shows the chat.
    var LegacyGame = React.createClass({
        componentDidMount: function () {
            $("body").prepend("<div id='game'>");
            $("#game").html(this.props.html);
            $("body").append("<div id='overlay'>");
            if (this.props.compat_handler)
                this.props.compat_handler(this.props.html);
        },
        render: function (data) {
            return <div className="chat">
                     <Chat />
                   </div>;
        }
    });

    // Container for server-side dialogs
    var Dialog = React.createClass({
        handle_click: function (ev) {
            if (ev.target.hasAttribute("data-key"))
            {
                ev.preventDefault();

                var key = ev.target.getAttribute("data-key").charCodeAt(0);
                comm.send_raw('{"msg":"input","data":[' + key + ']}');
            }
        },

        render: function () {
            var html = this.props.html;
            return <Overlay>
                     <div onClick={this.handle_click} className="server_dialog"
                          dangerouslySetInnerHTML={{__html: html}} />
                   </Overlay>;
        }
    });

    // Game loader
    //
    // Shows the loading screen, asks for login if necessary, and
    // receives the client and shows it.
    var GameLoader = React.createClass({
        getInitialState: function () {
            return {
                watching: null,
                game_component: null,
                dialog: null,
                state: "loading"
            };
        },
        componentDidMount: function () {
            comm.register_handlers({
                "stale_process_fail": this.stale_process_fail,
                "force_terminate?": this.force_terminate,

                "login_required": this.login_required,

                "show_dialog": this.show_dialog,
                "hide_dialog": this.hide_dialog,

                "game_started": this.game_started,
                "watching_started": this.watching_started,
                "game_ended": this.game_ended,
                "game_client": this.receive_game_client
            });
        },

        stale_process_fail: function (data) {
            this.setState({state: "failed",
                           error_msg: data.content});
        },
        force_terminate: function (data) {
            this.setState({state: "force_terminate"});
        },
        login_required: function (data) {
            this.setState({state: "logging_in",
                           logging_in_for: data.game});
        },
        show_dialog: function (data) {
            this.setState({dialog: data.html});
        },
        hide_dialog: function (data) {
            this.setState({dialog: null});
        },
        game_started: function () {
            this.setState({state: "play"});
        },
        watching_started: function (data) {
            this.setState({state: "play",
                           watching: data.username});
        },
        game_ended: function (data) {
            if (data.reason)
            {
                if (this.state.watching ||
                    normal_exit.indexOf(data.reason) === -1)
                {
                    this.setState({state: "ended",
                                   game_end: data});
                }
                else
                    document.location = "/";
            }
            else
                document.location = "/";
        },
        receive_game_client: function (data) {
            comm.inhibit_messages();
            var _this = this;
            if (data.content === undefined)
            {
                var path = "/gamedata/" + data.version;
                var module = "game" + (data.min ? ".min" : "");
                require.config({paths: {
                    "game_data": path,
                    "game_data/game": path + "/" + module
                }});
                require(["game_data/game"], function (game) {
                    _this.setState({game_component: game});
                    comm.uninhibit_messages();
                });
            }
            else
            {
                var version = data.content.match(/^<!-- +version: +(\d+) /);
                if (version == null)
                    version = 0;
                else
                    version = parseInt(version[1]);
                var do_load = function (compat)
                {
                    function create_game_component()
                    {
                        return LegacyGame({compat_handler: compat,
                                           html: data.content});
                    }
                    _this.setState({game_component: create_game_component});
                }

                if (version < 1)
                    require(["compat" + version], do_load);
                else
                    do_load(null);
            }
        },

        logged_in: function () {
            location.reload();
        },
        force_terminate_no: function () {
            comm.send_message("force_terminate", { answer: false });
            this.setState({state: "loading"});
        },
        force_terminate_yes: function () {
            comm.send_message("force_terminate", { answer: true });
            this.setState({state: "loading"});
        },

        render: function () {
            var game = null, overlay = null, stuff = null, dialog = null;

            if (this.state.game_component)
                game = this.state.game_component();

            if (this.state.state !== "play" &&
                this.state.state !== "ended")
            {
                var img_src = document.getElementById("loader_img").src;
                overlay = <Overlay opacity={1.0} className="noborder loader">
                            <span>Loading...</span><br />
                            <img src={img_src} />
                          </Overlay>;
            }

            switch (this.state.state)
            {
            case "logging_in":
                stuff = <Overlay className="login_overlay">
                          <h3>Login required</h3>
                          <div>You need to log in to play
                               {" " + this.state.logging_in_for}.</div>
                          <LoginForm on_login={this.logged_in} />
                        </Overlay>;
                break;

            case "failed":
                stuff = <Overlay>
                          <h3>Could not start game</h3>
                          <div dangerouslySetInnerHTML=
                               {{__html: this.state.error_msg}} />
                        </Overlay>;
                break;

            case "force_terminate":
                stuff = <Overlay>
                          <h3>Force terminate?</h3>
                          There is already a Crawl process running that could
                          not be stopped cleanly. Force termination?
                          <input type="button" value="No"
                            onClick={this.force_terminate_no} />
                          <input type="button" value="Yes"
                            onClick={this.force_terminate_yes}/>
                        </Overlay>;
                break;

            case "ended":
                var ge = this.state.game_end;
                var reason_msg = exit_reason_message(ge.reason,
                                                     this.state.watching);
                var dump_link = null, exit_msg = null;
                if (ge.message)
                {
                    exit_msg = <pre>{ge.message.replace(/\s+$/, "")}</pre>;
                }
                if (ge.dump)
                {
                    var text;
                    if (ge.reason === "saved")
                        text = "Character dump";
                    else if (ge.reason === "crash")
                        text = "Crash log";
                    else
                        text = "Morgue file";
                    dump_link = <a target="_blank" href={ge.dump + ".txt"}>
                                  {text}
                                </a>;
                }
                stuff = <Overlay>
                          <h3>The game ended.</h3>
                          <p>{reason_msg}</p>
                          {exit_msg}
                          <p style={{textAlign: "right"}}>{dump_link}</p>
                          <a href="/">Close</a>
                        </Overlay>;
                break;
            }

            if (this.state.dialog)
            {
                dialog = <Dialog html={this.state.dialog} />;
            }

            return <span>{game}{overlay}{stuff}{dialog}</span>;
        }
    });

    function possessive(name)
    {
        return name + "'" + (name.slice(-1) === "s" ? "" : "s");
    }

    function exit_reason_message(reason, watched_name)
    {
        if (watched_name)
        {
            switch (reason)
            {
            case "quit":
            case "won":
            case "bailed out":
            case "dead":
                return null;
            case "cancel":
                return watched_name + " quit before creating a character.";
            case "saved":
                return watched_name + " stopped playing (saved).";
            case "crash":
                return possessive(watched_name) + " game crashed.";
            case "error":
                return possessive(watched_name)
                       + " game was terminated due to an error.";
            default:
                return possessive(watched_name) + " game ended unexpectedly."
                       + (reason != "unknown" ? " (" + reason + ")" : "");
            }
        }
        else
        {
            switch (reason)
            {
            case "quit":
            case "won":
            case "bailed out":
            case "dead":
            case "saved":
            case "cancel":
                return null;
            case "crash":
                return "Unfortunately your game crashed.";
            case "error":
                return "Unfortunately your game terminated due to an error.";
            default:
                return "Unfortunately your game ended unexpectedly."
                       + (reason != "unknown" ? " (" + reason + ")" : "");
            }
        }
    }

    return GameLoader;
});
