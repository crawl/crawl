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

    return {
        Overlay: Overlay
    };
});
