/** @jsx React.DOM */
define(["react", "comm", "pubsub", "user", "jsx!login", "jsx!misc", "linkify"],
function (React, comm, pubsub, user, login, misc, linkify) {
    "use strict";

    var spectators = [];
    var anon_spec_count = 0;
    var messages = [];

    var Overlay = misc.Overlay;
    var LoginForm = login.LoginForm;

    // Container that automatically scrolls down when stuff is added,
    // but only if it was already at the bottom
    var ChatViewer = React.createClass({
        componentWillUpdate: function () {
            var n = this.getDOMNode();
            this.at_bottom = (n.scrollTop + n.offsetHeight) === n.scrollHeight;
        },
        componentDidUpdate: function () {
            if (this.at_bottom)
                this.scroll_bottom();
        },

        scroll_bottom: function () {
            var n = this.getDOMNode();
            n.scrollTop = n.scrollHeight;
        },

        render: function () {
            var container_style = {
                overflowX: "hidden",
                overflowY: "auto",
                textOverflow: "ellipsis",
                wordWrap: "break-word"
            };
            return this.transferPropsTo(
                <div style={container_style}>{this.props.children}</div>
            );
        }
    });

    // Chat component
    //
    // Chat data is managed in this module and the component is kept
    // updated via pubsub. Storing the chat messages as state would be
    // bad if the chat moves (not possible now, but possibly in the
    // future)
    var Chat = React.createClass({
        getInitialState: function () {
            return {
                folded: true,
                seen_msg_count: messages.length,
                msg: "",
                logging_in: false
            };
        },
        componentDidMount: function () {
            pubsub.subscribe("chat_update", this.update);
            pubsub.subscribe("chat_hotkey", this.focus);
        },
        componentWillUnmount: function () {
            pubsub.unsubscribe("chat_update", this.update);
            pubsub.unsubscribe("chat_hotkey", this.focus);
        },
        componentDidUpdate: function (prev_props, prev_state) {
            if ((prev_state.folded && !this.state.folded) ||
                (prev_state.logging_in && !this.state.logging_in))
            {
                if (this.refs.input)
                    this.refs.input.getDOMNode().focus();
            }
        },

        update: function () {
            if (!this.state.folded)
                this.setState({seen_msg_count: messages.length})
            else
                this.forceUpdate();
        },
        focus: function () {
            if (this.state.folded)
                this.setState({folded: false});
            else if (this.refs.input)
                this.refs.input.getDOMNode().focus();
        },

        handle_input: function (ev) {
            this.setState({msg: ev.target.value})
        },
        toggle: function () {
            this.setState({folded: !this.state.folded,
                           seen_msg_count: messages.length})
        },
        send: function (ev) {
            ev.preventDefault();
            if (this.state.msg !== "")
            {
                comm.send_message("chat_msg", {
                    text: this.state.msg
                });
            }
            this.setState({msg: ""});
            return false;
        },
        keydown: function (ev) {
            if (ev.key === "Escape")
            {
                ev.stopPropagation();
                this.refs.input.getDOMNode().blur();
                this.toggle();
            }
        },
        do_login: function () {
            this.setState({logging_in: true});
        },
        stop_login: function () {
            this.setState({logging_in: false});
        },

        render: function () {
            var body_style = {
                display: this.state.folded ? "none" : "block"
            };

            var new_msg_count = messages.length - this.state.seen_msg_count;
            var msgcount_class = "message_count";
            if (new_msg_count > 0)
                msgcount_class += " has_new";

            var specs = spectators.slice(0);
            specs.sort(spectator_compare);
            var speclist = [];
            var spec_count = 0;
            for (var i = 0; i < specs.length; ++i)
            {
                if (i > 0) speclist.push(", ");
                var cls = specs[i].player ? "player" : "watcher";
                speclist.push(<span className={cls}>{specs[i].name}</span>);
                if (!specs[i].player) ++spec_count;
            }

            var bottom;
            if (user.username)
            {
                bottom = <form action="">
                           <input type="text" onChange={this.handle_input}
                                  value={this.state.msg} ref="input" />
                           <input type="submit" onClick={this.send} value="OK" />
                         </form>;
            }
            else
            {
                bottom = <div className="chat_login_text">
                           <a onClick={this.do_login} href="javascript:">
                             Login</a>
                           {" to chat"}
                         </div>;
            }

            var login = null;
            if (this.state.logging_in)
            {
                login = <Overlay on_cancel={this.stop_login}
                                 className="login_overlay">
                          <h3>Login</h3>
                          <LoginForm on_login={this.stop_login} />
                        </Overlay>;
            }

            return <span>
                     <a href="javascript:" className="caption"
                        onClick={this.toggle}>
                       <span className="spectator_count">
                         {spec_count + " spectators"}
                       </span>
                       <span className={msgcount_class}>
                         {new_msg_count + " new messages"}
                         {new_msg_count > 0 ? " (Press F12)" : ""}
                       </span>
                     </a>
                     <div className="chat_body" style={body_style}
                          onKeyDown={this.keydown}>
                       <span className="spectator_list">{speclist}</span>
                       <ChatViewer className="chat_container">
                         <span className="chat_history" style={{width: "100%"}}>
                           {messages.map(format_msg)}
                         </span>
                       </ChatViewer>
                       {bottom}
                     </div>
                     {login}
                   </span>;
        }
    });


    comm.register_handlers({
        "dump": handle_dump,
        "chat": receive_message,
        "update_spectators": update_spectators
    });


    function handle_dump(data)
    {
        messages.push({text: data.url + ".txt"});
    }

    function receive_message(data)
    {
        delete data["msg"];
        messages.push(data);
        pubsub.publish("chat_update");
    }

    function update_spectators(data)
    {
        spectators = data.spectators;
        anon_spec_count = data.anon_count;
        pubsub.publish("chat_update");
    }


    function format_msg(msg)
    {
        if (msg.sender === undefined)
        {
            return <div>
                     <span className="chat_msg chat_automated">
                       {linkify(msg.text)}
                     </span>
                   </div>;
        }
        else
        {
            return <div>
                     <span className="chat_sender">{msg.sender}</span>
                     {": "}
                     <span className="chat_msg">{linkify(msg.text)}</span>
                   </div>;
        }
    }

    function spectator_compare(a, b)
    {
        if (a.player !== b.player)
            return a.player ? -1 : 1;
        return (a.name < b.name) ? -1 : (a.name === b.name) ? 0 : 1;
    }

    return Chat;
});
