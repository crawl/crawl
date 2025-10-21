// Bundling styles didn't work, because there's no good way to *unload* them
// when we switch game client. This could probably be handled properly but for
// now including them with plain old html is the path of least resistance.
// import "./style.css";
// import "./simplebar.css";
import "./game";

import client from "client";
import $ from "jquery";

// Game initialisation (moved from inline script in html template)
$(document).trigger("game_preinit");
$(document).trigger("game_init");
client.uninhibit_messages();
