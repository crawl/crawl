/** @jsx React.DOM */
define(["react", "comm", "pubsub", "user", "jsx!misc", "jsx!login", "jquery"],
function (React, comm, pubsub, user, misc, login, $) {
    "use strict";

    var extend = $.extend;

    // Score navigation dropdwon
    var ScoreNavigation = React.createClass({
        getDefaultProps: function () {
            return {
                initial_num_scores: 500,
                edit_num_scores: true
            };
        },

        getInitialState: function () {
            var game_version = null;
            var game_mode = null;
            var map_name = null;
            if (this.props.games)
            {
                game_mode = user.abbrev_mode_name(this.props.games[0].mode);
                game_version = this.props.games[0].version;
                if (this.props.games[0].hasOwnProperty("game_maps"))
                    map_name = this.props.games[0].game_maps[0].name;
                else
                    map_name = game_mode;
            }
            return {
                game_version: game_version,
                game_mode: game_mode,
                map_name: map_name,
                num_scores: this.props.initial_num_scores
            };
        },

        select_version : function (event)
        {
            this.setState({game_version: event.target.value});
        },

        select_map : function (event)
        {
            var res = event.target.value.split(":", 2);
            var game_mode = res[0];
            var map_name = null;
            if (res.length > 1)
                map_name = res[1];
            this.setState({game_mode : game_mode,
                           map_name : map_name});
        },

        set_num_scores : function (event)
        {
            this.setState({num_scores: event.target.value});
        },

        load_scores : function(ev)
        {
            if (history)
            {
                var url = "/scores/top" + this.state.num_scores + "/"
                    + this.state.game_version + "/" + this.state.game_mode;
                if (this.state.map_name !== this.state.game_mode)
                    url += "/" + this.state.map_name;
                history.pushState(null, "", url);
                pubsub.publish("url_change");
            }
            ev.preventDefault();
        },

        render: function () {
            function make_version_option(version)
            {
                return <option value={version}>{version}</option>;
            }

            function make_map_option(entry)
            {
                return <option value={entry.game_mode + ":" + entry.map_name}>
                         {entry.map_desc}
                       </option>;
            }

            var versions = user.collect_versions(this.props.games);
            var map_list = user.collect_maps(this.props.games,
                                             this.state.game_version);
            var ns = null;
            if (this.props.edit_num_scores)
            {
                ns = <span>Num:
                       <input type="text"
                              onChange={this.set_num_scores.bind(this)}
                              value={this.state.num_scores} size="4"/>
                     </span>;
            }
            var f = <form action="">
                       <select name="game_map"
                               onChange={this.select_map.bind(this)}>
                         {map_list.map(make_map_option)}
                       </select>
                       <select name="game_version"
                               onChange={this.select_version.bind(this)} >
                         {versions.map(make_version_option)}
                       </select>
                       {ns}
                       <input type="submit" onClick={this.load_scores.bind(this)}
                              value="View" />
                     </form>;
            return <div><span>Server Rankings:</span>{f}</div>;
        }
    });

    return {ScoreNavigation : ScoreNavigation};
});
