/** @jsx React.DOM */
define(["react", "jsx!misc", "comm", "pubsub", "user"],
function (React, misc, comm, pubsub, user) {
    "use strict";

    var Overlay = misc.Overlay;

    // broadcast fail messages so they can be received by any login form
    comm.register_handler("register_fail", function (data) {
        pubsub.publish("register_fail", data.reason);
    });
    comm.register_handler("login_fail", function (data) {
        pubsub.publish("login_fail");
    });

    // Registration form
    var RegisterForm = React.createClass({
        getInitialState: function () {
            return {
                message: "",
                error: false,
                username: "",
                password: "",
                password_repeat: "",
                email: ""
            };
        },
        componentDidMount: function () {
            pubsub.subscribe("logged_in", this.logged_in);
            pubsub.subscribe("register_fail", this.fail);
            this.refs.username.getDOMNode().focus();
        },
        componentWillUnmount: function () {
            pubsub.unsubscribe("logged_in", this.logged_in);
            pubsub.unsubscribe("register_fail", this.fail);
        },

        logged_in: function () {
            if (this.props.on_finished)
                this.props.on_finished();
        },
        fail: function (reason) {
            this.setState({
                message: reason,
                error: true
            });
        },

        update: function (ev) {
            var obj = {};
            obj[ev.target.getAttribute("name")] = ev.target.value;
            this.setState(obj);
        },
        cancel: function (ev) {
            if (this.props.on_finished)
                this.props.on_finished();
        },
        send: function (ev) {
            ev.preventDefault();
            if (this.state.username.indexOf(" ") >= 0)
            {
                this.setState({
                    message: "The username can't contain spaces.",
                    error: true
                });
            }
            else if (this.state.email.indexOf(" ") >= 0)
            {
                this.setState({
                    message: "The email can't contain spaces.",
                    error: true
                });
            }
            else if (this.state.password !== this.state.password_repeat)
            {
                this.setState({
                    message: "Passwords don't match.",
                    error: true
                });
            }
            else
            {
                this.setState({
                    message: "...",
                    error: false
                });
                comm.send_message("register", {
                    username: this.state.username,
                    password: this.state.password,
                    email: this.state.email
                });
            }
        },

        render: function () {
            var cls = this.state.error ? "error" : "";
            var msg = <span className={cls}>{this.state.message}<br /></span>;

            var d = <form action="">
                      <div>
                        <label for="username">Username:</label>
                        <input type="text" name="username"
                               onChange={this.update} ref="username"
                               value={this.state.username} />
                      </div>
                      <div>
                        <label for="email">Email address:</label>
                        <input type="email" name="email"
                               onChange={this.update}
                               value={this.state.email} />
                      </div>
                      <div>
                        <label for="password">Password:</label>
                        <input type="password"
                               name="password" onChange={this.update}
                               value={this.state.password} />
                      </div>
                      <div>
                        <label for="password_repeat">Repeat password:</label>
                        <input type="password" name="password_repeat"
                               onChange={this.update}
                               value={this.state.password_repeat}/>
                      </div>
                      <div>
                        <input type="button" name="cancel"
                               value="Cancel" onClick={this.cancel} />
                        <input type="submit" name="submit"
                               value="Submit" onClick={this.send} />
                      </div>
                    </form>;
            return <div className="register_form"
                        style={{overflow: "hidden"}}>{msg}{d}</div>;
        }
    });

    // Login form
    //
    // Both for the top right corner of the lobby, and various login overlays.
    // Includes a link to open the registration form in an overlay.
    var LoginForm = React.createClass({
        getInitialState: function () {
            return {
                username: "",
                password: "",
                rememberme: false,
                registering: false,
                failed: false
            };
        },
        componentDidMount: function () {
            pubsub.subscribe("logged_in", this.logged_in);
            pubsub.subscribe("login_fail", this.login_fail);
            this.refs.username.getDOMNode().focus();
        },
        componentWillUnmount: function () {
            pubsub.unsubscribe("logged_in", this.logged_in);
            pubsub.unsubscribe("login_fail", this.login_fail);
        },

        logged_in: function (username) {
            if (this.props.on_login)
                this.props.on_login(username);
        },
        login_fail: function () {
            this.setState({failed: true});
        },

        handle_username: function (ev) {
            this.setState({username: ev.target.value});
        },
        handle_password: function (ev) {
            this.setState({password: ev.target.value});
        },
        handle_rememberme: function (ev) {
            this.setState({rememberme: ev.target.checked});
        },
        send: function (ev) {
            user.login(this.state.username, this.state.password,
                       this.state.rememberme);
            ev.preventDefault();
        },
        register: function () {
            this.setState({registering: true});
        },
        stop_registering: function () {
            this.setState({registering: false});
        },

        render: function () {
            var fail = null;
            if (this.state.failed)
                fail = <div className="error">Login failed!</div>;
            var f = <form action="">
                      {fail}
                      <div>
                        <input type="text" name="username"
                               placeholder="User"
                               value={this.state.username}
                               onChange={this.handle_username} ref="username" />
                      </div>
                      <div>
                        <input type="password" name="password"
                               placeholder="Password"
                               value={this.state.password}
                               onChange={this.handle_password} />
                      </div>
                      <div>
                        <input type="checkbox" name="rememberme"
                               checked={this.state.rememberme}
                               onChange={this.handle_rememberme} />
                        <label htmlFor="rememberme">Stay logged in</label>
                      </div>
                      <input type="submit" value="Login"
                             onClick={this.send} />
                    </form>;
            var reglink = <a href="javascript:"
                             onClick={this.register}>Register</a>;
            var reg = null;
            if (this.state.registering)
            {
                reg = <Overlay on_cancel={this.stop_registering}>
                        <h3>Register</h3>
                        <RegisterForm on_finished={this.stop_registering} />
                      </Overlay>;
            }
            return <div>{this.props.message}{f}{reglink}{reg}</div>;
        }
    });

    return {
        RegisterForm: RegisterForm,
        LoginForm: LoginForm
    };
});
