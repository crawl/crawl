import $ from "jquery";
import "./contrib/ba-linkify.min";

const ALLOWED_PROTOCOLS = ["http", "https", "ftp", "irc"];

const ba_linkify = window.linkify;
delete window.linkify;

function escape_html(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function linkify(text) {
  return ba_linkify(text, {
    callback: (text, href) => {
      if (!href) return escape_html(text);
      if (!ALLOWED_PROTOCOLS.some((p) => href.indexOf(`${p}://`) === 0)) {
        return escape_html(text);
      }
      return $("<a>").attr("href", href).attr("target", "_blank").text(text)[0]
        .outerHTML;
    },
  });
}

export default linkify;
