var crtContext, dungeonContext;
var rows = 24, cols = 80;
var crtCellWidth, crtCellHeight = 17;
var font = "16px monospace";

var dungeonCellWidth = 32, dungeonCellHeight = 32;
var dungeonCols = 0, dungeonRows = 0;

var cursorX = 0, cursorY = 0;

var socket;

function assert(cond) {
    if (!cond)
        console.log("Assertion failed!");
}

function drawTextCell(cx, cy, content) {
    x = cx * crtCellWidth;
    y = cy * crtCellHeight;
    crtContext.fillStyle = "black";
    crtContext.fillRect(x, y, crtCellWidth, crtCellHeight);
    crtContext.fillStyle = "white";
    crtContext.font = font;
    crtContext.fillText(content, x + crtCellWidth / 2, y + crtCellHeight / 2);
}

function cgotoxy(cx, cy) {
    cursorX = cx;
    cursorY = cy;
}

function putch(ch) {
    drawTextCell(cursorX, cursorY, ch);
    cursorX++;
}

function puts(str) {
    for (i = 0; i < str.length; ++i) {
        putch(str.charAt(i));
    }
}

function clrscr() {
    crtContext.fillStyle = "black";
    crtContext.fillRect(0, 0, crtCellWidth * cols, crtCellHeight * rows);
}

function setLayer(layer) {
    if (layer == "crt") {
        $("#crt").show();
        $("#dungeon").hide();
    }
    else if (layer == "normal") {
        $("#crt").hide();
        $("#dungeon").show();
    }
}

function getImg(id) {
    return $("#" + id)[0];
}

function viewSize(cols, rows) {
    if ((cols == dungeonCols) && (rows == dungeonRows))
        return;
    console.log("New viewsize: " + cols + "/" + rows);
    dungeonCols = cols;
    dungeonRows = rows;
    canvas = $("#dungeon")[0];
    canvas.width = dungeonCols * dungeonCellWidth;
    canvas.height = dungeonRows * dungeonCellHeight;
    dungeonContext = canvas.getContext("2d");
}

function bg(cx, cy, bg) {
    console.log("bg("+bg+")");
    bg_idx = bg & TILE_FLAG_MASK;
    x = dungeonCellWidth * cx;
    y = dungeonCellHeight * cy;
    img = getImg(getdngnImg(bg_idx));
    info = getdngnTileInfo(bg_idx);
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h, x + info.ox, y + info.oy, w, h);
}

function fg(cx, cy, fg) {
    console.log("fg("+fg+")");
    fg_idx = fg & TILE_FLAG_MASK;
    info = getmainTileInfo(fg_idx);
    img = getImg("main");
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h,
                             x + info.ox, y + info.oy, w, h);
}

function p(cx, cy, part, ofsx, ofsy) {
    console.log("p("+part+")");
    info = getplayerTileInfo(part);
    img = getImg("player");
    w = info.ex - info.sx;
    h = info.ey - info.sy;
    dungeonContext.drawImage(img, info.sx, info.sy, w, h,
                             x + info.ox + (ofsx || 0), y + info.oy + (ofsy || 0), w, h);
}

function handleKeypress(e) {
    socket.send(String.fromCharCode(e.which));
}

function handleKeydown(e) {
    cgotoxy(0, 0);
    puts(e.which + "");
}

$(document).ready(function() {
    crtCanvas = $("#crt")[0];

    crtContext = crtCanvas.getContext("2d");

    // Measure font
    crtContext.font = font;
    metrics = crtContext.measureText("@");
    crtCellWidth = metrics.width;

    crtCanvas.width = cols * crtCellWidth;
    crtCanvas.height = rows * crtCellHeight;

    crtContext = crtCanvas.getContext("2d");
    crtContext.font = font;
    crtContext.textAlign = "center";
    crtContext.textBaseline = "middle";

    setLayer("crt");

    // Key handler
    $(document).bind('keypress.client', handleKeypress);
    $(document).bind('keydown.client', handleKeydown);

    socket = new WebSocket("ws://localhost:8080/socket");

    socket.onopen = function() {
    }

    socket.onmessage = function(msg) {
        eval(msg.data);
    }

    socket.onclose = function() {
    }
});
