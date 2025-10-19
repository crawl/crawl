define([ // These are in the order of the TextureID enum in tiles.h
        "./tileinfo-floor",
        "./tileinfo-wall",
        "./tileinfo-feat",
        "./tileinfo-player",
        "./tileinfo-main",
        "./tileinfo-gui",
        "./tileinfo-icons"
], function () {
    "use strict";

    var args = Array.prototype.slice.call(arguments);

    return function (tex_idx) {
        return args[tex_idx];
    };
});
