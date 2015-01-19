/*!
 * jQuery Browser Plugin 0.0.7
 * https://github.com/gabceb/jquery-browser-plugin
 *
 * Original jquery-browser code Copyright 2005, 2013 jQuery Foundation, Inc. and other contributors
 * http://jquery.org/license
 *
 * Modifications Copyright 2014 Gabriel Cebrian
 * https://github.com/gabceb
 *
 * Released under the MIT license
 *
 * Date: 25-12-2014
 */!function(a){"function"==typeof define&&define.amd?define(["jquery"],function(b){a(b)}):"object"==typeof module&&"object"==typeof module.exports?module.exports=a(require("jquery")):a(jQuery)}(function(a){"use strict";var b,c;if(a.uaMatch=function(a){a=a.toLowerCase();var b=/(edge)\/([\w.]+)/.exec(a)||/(opr)[\/]([\w.]+)/.exec(a)||/(chrome)[ \/]([\w.]+)/.exec(a)||/(version)(applewebkit)[ \/]([\w.]+).*(safari)[ \/]([\w.]+)/.exec(a)||/(webkit)[ \/]([\w.]+).*(version)[ \/]([\w.]+).*(safari)[ \/]([\w.]+)/.exec(a)||/(webkit)[ \/]([\w.]+)/.exec(a)||/(opera)(?:.*version|)[ \/]([\w.]+)/.exec(a)||/(msie) ([\w.]+)/.exec(a)||a.indexOf("trident")>=0&&/(rv)(?::| )([\w.]+)/.exec(a)||a.indexOf("compatible")<0&&/(mozilla)(?:.*? rv:([\w.]+)|)/.exec(a)||[],c=/(ipad)/.exec(a)||/(ipod)/.exec(a)||/(iphone)/.exec(a)||/(kindle)/.exec(a)||/(silk)/.exec(a)||/(android)/.exec(a)||/(windows phone)/.exec(a)||/(win)/.exec(a)||/(mac)/.exec(a)||/(linux)/.exec(a)||/(cros)/.exec(a)||/(playbook)/.exec(a)||/(bb)/.exec(a)||/(blackberry)/.exec(a)||[];return{browser:b[5]||b[3]||b[1]||"",version:b[2]||b[4]||"0",versionNumber:b[4]||b[2]||"0",platform:c[0]||""}},b=a.uaMatch(window.navigator.userAgent),c={},b.browser&&(c[b.browser]=!0,c.version=b.version,c.versionNumber=parseInt(b.versionNumber,10)),b.platform&&(c[b.platform]=!0),(c.android||c.bb||c.blackberry||c.ipad||c.iphone||c.ipod||c.kindle||c.playbook||c.silk||c["windows phone"])&&(c.mobile=!0),(c.cros||c.mac||c.linux||c.win)&&(c.desktop=!0),(c.chrome||c.opr||c.safari)&&(c.webkit=!0),c.rv||c.edge){var d="msie";b.browser=d,c[d]=!0}if(c.safari&&c.blackberry){var e="blackberry";b.browser=e,c[e]=!0}if(c.safari&&c.playbook){var f="playbook";b.browser=f,c[f]=!0}if(c.bb){var g="blackberry";b.browser=g,c[g]=!0}if(c.opr){var h="opera";b.browser=h,c[h]=!0}if(c.safari&&c.android){var i="android";b.browser=i,c[i]=!0}if(c.safari&&c.kindle){var j="kindle";b.browser=j,c[j]=!0}if(c.safari&&c.silk){var k="silk";b.browser=k,c[k]=!0}return c.name=b.browser,c.platform=b.platform,a.browser=c,c});