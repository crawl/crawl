import $ from "jquery";
import simplebar from "./contrib/simplebar";

export default function (el) {
  let scroller = $(el).data("scroller");
  if (scroller === undefined) {
    const $el = $(el);
    scroller = new simplebar(el, { autoHide: false });
    $el.data("scroller", scroller);

    const scrollElement = scroller.getScrollElement();
    const contentElement = scroller.getContentElement();
    const top = $("<div class='scroller-shade top'>"),
      bot = $("<div class='scroller-shade bot'>"),
      lhd = $("<span class='scroller-lhd'>_</span>");
    $el.append(top).append(bot).append(lhd);
    const update_shades = () => {
      const dy = $(scrollElement).scrollTop(),
        ch = $(contentElement).outerHeight(),
        sh = $(scrollElement).outerHeight();
      top.css("opacity", dy / 100 / 4);
      bot.css("opacity", (ch - sh - dy) / 100 / 4);
    };
    scrollElement.addEventListener("scroll", update_shades);
    update_shades();
  }
  return {
    contentElement: scroller.getContentElement(),
    scrollElement: scroller.getScrollElement(),
  };
}
