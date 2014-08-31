/** @jsx React.DOM */
define(["react", "comm", "pubsub", "user", "jsx!misc", "jsx!login",
        "jsx!scorenav", "jquery"],
function (React, comm, pubsub, user, misc, login, scorenav, $) {
    "use strict";

    var extend = $.extend;

    var LoginForm = login.LoginForm;
    var PasswordChangeLink = login.PasswordChangeLink;
    var LogoutLink = login.LogoutLink;
    var ScoreNavigation = scorenav.ScoreNavigation;

    // One line in the list of running games
    var ScoreEntry = React.createClass({
        render: function () {
            var s = this.props.score;
            var title = user.nerd_description(s.name, s.nerdtype[0],
                                              s.nerdtype[1]);
            var morgue = <a href={s.morgue_url} className={s.nerdtype[0]}
                            title={title}>{s.name}</a>;
            return <tr>
                     <td>{s.rank}</td>
                     <td>{s.score}</td>
                     <td>{morgue}</td>
                     <td>{s.char}</td>
                     <td>{s.god}</td>
                     <td>{s.title}</td>
                     <td>{s.place}</td>
                     <td>{s.xl}</td>
                     <td>{s.turns}</td>
                     <td>{s.runes}</td>
                     <td>{s.date}</td>
                   </tr>;
        }
    });

    // The list of scores
    var ScoresList = React.createClass({
        getInitialState: function () {
            return {
                sort: [{field: "rank", down: false}]
            };
        },

        header_click: function (ev) {
            var field = ev.target.getAttribute("data-field");
            var sort = this.state.sort;
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
            var data = this.props.scores.slice(0), sort = this.state.sort;
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
                            {
                                result = parseInt(parts_a[1])
                                    - parseInt(parts_b[1]);
                            }
                        }
                        else if (f === "turns" || f === "score")
                        {
                            x = x.replace(",", "");
                            y = y.replace(",", "");
                            x = parseInt(x) || -1;
                            y = parseInt(y) || -1;
                            result = x - y;
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
            function make_entry(score)
            {
                return <ScoreEntry score={score} />;
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
                        <tr onClick={this.header_click.bind(this)}>
                          {h("rank", "Rank")}
                          {h("score", "Score")}
                          {h("name", "Name")}
                          {h("char", "Char")}
                          {h("god", "God")}
                          {h("title", "Title")}
                          {h("place", "Place")}
                          {h("xl", "XL")}
                          {h("turns", "Turns")}
                          {h("runes", "Runes")}
                          {h("date", "Date")}
                        </tr>
                      </thead>
                      <tbody>
                        {data.map(make_entry)}
                      </tbody>
                    </table>;
            return <div><span>Scores for {this.props.game_desc}:</span>{t}</div>;
        }
    });


    // Root component for the scores
    var Scores = React.createClass({
        getInitialState: function () {
            return {
                username: null,
                num_scores: 0,
                scores: null,
                games: null,
                game_id: null,
                state: "loading"
            };
        },
        componentDidMount: function () {
            comm.register_handler("game_info", this.game_info);
            comm.register_handler("scores", this.load_scores);
            comm.send_message("get_game_info");
        },

        componentWillUnmount: function () {
            comm.register_handler("game_info", this.game_info);
            comm.unregister_handler("scores", this.load_scores);
        },

        game_info: function (data) {
            this.setState({games: data.games});
        },

        load_scores: function (data) {
            this.setState({scores : data.scores, state : "received",
                           game_desc : data.game_desc});
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
                               Logged in as {this.state.username}{" | "}
                               <PasswordChangeLink />{" | "}
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

            var score_nav = null;
            if (this.state.games)
                score_nav = <ScoreNavigation games={this.state.games} />;
            var score_entries = "Loading scores...";
            if (this.state.scores)
            {
                score_entries = <ScoresList scores={this.state.scores}
                                            game_desc={this.state.game_desc}/>;
            }
            else if (this.state.game_desc !== null)
                score_entries = "No scores found for " + this.state.game_desc;
            else
                score_entries = "Invalid score request";
            return <div className="scores">
                     {login_line}
                     {score_nav}
                     {score_entries}
                   </div>;
        }
    });

    return Scores;
});
