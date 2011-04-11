var currentLayer = "crt";

var cursorX = 0, cursorY = 0;
var fgCol = 16, bgCol = 0;

var dungeonContext;
var dungeonCellWidth = 32, dungeonCellHeight = 32;
var dungeonCols = 0, dungeonRows = 0;

var socket;

function assert(cond) {
    if (!cond)
        console.log("Assertion failed!");
}

function setLayer(layer) {
    if (layer == "crt") {
        $("#crt").show();
        $("#dungeon").hide();
        $("#stats").hide();
        $("#messages").hide();
    }
    else if (layer == "normal") {
        $("#crt").hide();
        $("#dungeon").show();
        // jQuery should restore display correctly -- but doesn't
        $("#stats").css("display", "inline-block");
        $("#messages").show();
    }
    currentLayer = layer;
}

function getImg(id) {
    return $("#" + id)[0];
}

function viewSize(cols, rows) {
    if ((cols == dungeonCols) && (rows == dungeonRows))
        return;
    dungeonCols = cols;
    dungeonRows = rows;
    canvas = $("#dungeon")[0];
    canvas.width = dungeonCols * dungeonCellWidth;
    canvas.height = dungeonRows * dungeonCellHeight;
    dungeonContext = canvas.getContext("2d");
}

function shift(x, y) {
    x *= dungeonCellWidth;
    y *= dungeonCellHeight;
    if (x > 0) {
        sx = x;
        dx = 0;
    }
    else {
        sx = 0;
        dx = -x;
    }
    if (y > 0) {
        sy = y;
        dy = 0;
    }
    else {
        sy = 0;
        dy = -y;
    }
    w = (dungeonCols * dungeonCellWidth - abs(x));
    h = (dungeonRows * dungeonCellHeight - abs(y));

    dungeonContext.drawImage($("#dungeon")[0], sx, sy, w, h, dx, dy, w, h);

    dungeonContext.fillStyle = "black";
    dungeonContext.fillRect(0, 0, dungeonCols * dungeonCellWidth, dy);
    dungeonContext.fillRect(0, dy, dx, h);
    dungeonContext.fillRect(w, 0, sx, dungeonRows * dungeonCellHeight);
    dungeonContext.fillRect(0, h, w, sy);
}

function handleKeypress(e) {
    s = String.fromCharCode(e.which);
    if (s == "\\") {
        socket.send("\\92\n");
    } else if (s == "^") {
        socket.send("\\94\n");
    }
    else
        socket.send(s);
}

function handleKeydown(e) {
    if (e.which in keyConversion) {
        socket.send("\\" + keyConversion[e.which] + "\n");
    }
    else
        console.log("Key: " + e.which);
}

$(document).ready(function() {
    setLayer("crt");

    // Key handler
    $(document).bind('keypress.client', handleKeypress);
    $(document).bind('keydown.client', handleKeydown);

    socket = new WebSocket("ws://localhost:8080/socket");

    socket.onopen = function() {
    }

    socket.onmessage = function(msg) {
        try {
            eval(msg.data);
        } catch (err) {
            console.error("Error in message: " + msg.data + " - " + err);
        }
    }

    socket.onclose = function() {
    }
});

function abs(x) { return x > 0 ? x : -x; }
