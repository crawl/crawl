/** @jsx React.DOM */
define(["react"], function (React) {
    "use strict";

    // Overlay component
    //
    // Shows its children in an overlay in the middle of the screen.
    // Handles "cancelling" the overlay via escape key or clicking outside.
    var Overlay = React.createClass({
        cancel: function () {
            if (this.props.on_cancel)
                this.props.on_cancel();
        },
        keydown: function (ev) {
            if (ev.key === "Escape")
            {
                ev.stopPropagation();
                this.cancel();
            }
        },
        render: function () {
            var bgstyle = {
                position: "fixed",
                left: 0,
                top: 0,
                width: "100%",
                height: "100%",
                backgroundColor: "black",
                opacity: this.props.opacity || 0.7,
                zIndex: 2000
            };
            var style = {
                position: "fixed",
                zIndex: 2000,
                left: 0,
                top: 0,
                width: "100%",
                pointerEvents: "none"
            };
            var cls = "overlay " + (this.props.className || "");
            return (
              <span>
                <div style={bgstyle} onClick={this.cancel} />
                <div style={style} onKeyDown={this.keydown}>
                  <div className={cls} style={{pointerEvents: "auto"}}>
                    {this.props.children}
                  </div>
                </div>
              </span>
            );
        }
    });

    // Confirm button
    //
    // A button or link that asks for confirmation.
    var ConfirmClick = React.createClass({
        propTypes: {
            children: React.PropTypes.component.isRequired,
            on_confirm: React.PropTypes.func.isRequired,
            text: React.PropTypes.string.isRequired,
            title: React.PropTypes.string
        },

        getInitialState: function () {
            return {confirming: false};
        },

        cancel: function () {
            this.setState({confirming: false});
        },
        confirm: function () {
            this.setState({confirming: false});
            this.props.on_confirm();
        },
        click: function (ev) {
            ev.preventDefault();
            ev.stopPropagation();
            this.setState({confirming: true});
        },

        render: function () {
            var dialog = null;
            if (this.state.confirming)
            {
                dialog = (
                  <Overlay on_cancel={this.cancel} className="confirm">
                    <h3>{this.props.title || "Please confirm"}</h3>
                    <div>{this.props.text}</div>
                    <input type="button" value="Cancel" onClick={this.cancel} />
                    <input type="button" value="OK" onClick={this.confirm} />
                  </Overlay>
                );
            }
            return this.transferPropsTo(
              <span>
                <span onClick={this.click}>{this.props.children}</span>
                {dialog}
              </span>
            );
        }
    });

    return {
        Overlay: Overlay,
        ConfirmClick: ConfirmClick
    };
});
